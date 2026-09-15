#include "plexos.h"

/* CPU Intel: supporto completo i3-1115G4 (Tiger Lake-UP3 fam 6 mod 0x8C).
   - CPUID: vendor/brand/famiglia/cache/frequenze/feature
   - Abilita FPU/SSE via CR0/CR4 (dopo verifica)
   - PAT write-combining per il framebuffer (veloce a 1080p su HW reale)
   - Fallback QEMU ovunque: mai hang, mai fault. */

static char vendor_str[13] = "Unknown";
static char brand_str[49] = "Unknown CPU";
static char feat_str[160] = "base";
static u32 family=0, model=0, stepping=0;
static u32 mhz=0, base_mhz=0, max_mhz=0;
static u32 l1kb=0, l2kb=0, l3kb=0;
static int cores=1;
static int has_cpuid=0;
static int has_pat=0, has_sse=0, has_apic=0, has_invtcs=0, sse_on=0, pat_wc_on=0;

static inline void cpuid(u32 leaf, u32 sub, u32* a, u32* b, u32* c, u32* d){
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub));
}
static inline u64 rdtsc(void){
    u32 lo,hi; __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi)); return ((u64)hi<<32)|lo;
}
static u32 pit_count(void){
    outb(0x43, 0x00); /* latch ch0 */
    u32 lo=inb(0x40), hi=inb(0x40);
    return (hi<<8)|lo;
}
static inline u64 rdmsr(u32 msr){
    u32 lo,hi; __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(msr)); return ((u64)hi<<32)|lo;
}
static inline void wrmsr(u32 msr, u64 v){
    __asm__ volatile("wrmsr"::"a"((u32)v),"d"((u32)(v>>32)),"c"(msr));
}

void cpu_init(void){
    u32 e1,e2;
    __asm__ volatile("pushfl; popl %0; mov %0,%1; xor $0x200000,%1; pushl %1; popfl; pushfl; popl %1; pushl %0; popfl":"=r"(e1),"=r"(e2));
    if(((e1^e2)&0x200000)==0){ kprintf("[CPU] CPUID non disponibile\n"); return; }
    has_cpuid=1;
    u32 a,b,c,d;
    u32 maxleaf;
    cpuid(0,0,&maxleaf,&b,&c,&d);
    ((u32*)vendor_str)[0]=b; ((u32*)vendor_str)[1]=d; ((u32*)vendor_str)[2]=c; vendor_str[12]=0;
    cpuid(1,0,&a,&b,&c,&d);
    u32 f1a=a, f1b=b, f1c=c, f1d=d;
    stepping=f1a&0xF; model=(f1a>>4)&0xF; family=(f1a>>8)&0xF;
    if(family==6||family==15){
        if(family==15) family+=((f1a>>20)&0xFF);
        model|=((f1a>>12)&0xF0);
        if(family==6) model|=((f1a>>16)&0xF0);
    }
    has_sse=(f1d&(1<<25))&&((f1d&(1<<26)));
    has_pat=(f1d&(1<<16))&&(f1d&(1<<5)); /* PAT + MSR */
    has_apic=(f1d&(1<<9))!=0;
    /* brand */
    cpuid(0x80000000,0,&a,&b,&c,&d);
    if(a>=0x80000004){
        char* p=brand_str;
        for(u32 i=0;i<3;i++){
            cpuid(0x80000002+i,0,&a,&b,&c,&d);
            ((u32*)p)[0]=a;((u32*)p)[1]=b;((u32*)p)[2]=c;((u32*)p)[3]=d; p+=16;
        }
        brand_str[48]=0;
        while(brand_str[0]==' '){ memmove(brand_str,brand_str+1,48); }
    } else {
        strcpy(brand_str,"CPU 32-bit");
    }
    cpuid(0x80000007,0,&a,&b,&c,&d);
    has_invtcs=(d&(1<<8))!=0;
    /* cores: HTT + leaf 0xB */
    cores=1;
    if(f1d&(1<<28)){
        int hc=(f1b>>16)&0xFF;
        if(hc>1&&hc<=32) cores=hc;
    }
    if(maxleaf>=0xB){
        cpuid(0xB,0,&a,&b,&c,&d);
        if(b&0xFFFF){ int lc=b&0xFFFF; if(lc>1&&lc<=32) cores=lc; }
    }
    /* frequenze leaf 0x16 (base/max/bus, presente su Tiger Lake) */
    if(maxleaf>=0x16){
        cpuid(0x16,0,&a,&b,&c,&d);
        base_mhz=a&0xFFFF; max_mhz=b&0xFFFF;
    }
    /* cache leaf 4 deterministica */
    if(maxleaf>=4){
        for(u32 i=0;i<6;i++){
            cpuid(4,i,&a,&b,&c,&d);
            u32 type=a&0x1F;
            if(type==0) break;
            u32 level=(a>>5)&0x7;
            u32 ways=((b>>22)&0x3FF)+1;
            u32 parts=((b>>12)&0x3FF)+1;
            u32 line=(b&0xFFF)+1;
            u32 sets=c+1;
            u32 kb=(ways*parts*line*sets)/1024;
            if(type==1){ if(level==1) l1kb+=kb; }        /* dati */
            else if(type==2){ if(level==1) l1kb+=kb; }   /* istruzioni -> somma L1 */
            else if(type==3){ if(level==2) l2kb=kb; else if(level==3) l3kb=kb; }
        }
    }
    /* feature string */
    {
        char* p=feat_str; p[0]=0;
        if(f1d&(1<<23)) strcat(p,"MMX ");
        if(f1d&(1<<25)) strcat(p,"SSE ");
        if(f1d&(1<<26)) strcat(p,"SSE2 ");
        if(f1c&1) strcat(p,"SSE3 ");
        if(f1c&(1<<9)) strcat(p,"SSSE3 ");
        if(f1c&(1<<19)) strcat(p,"SSE4.1 ");
        if(f1c&(1<<20)) strcat(p,"SSE4.2 ");
        if(f1c&(1<<28)) strcat(p,"AVX ");
        if(f1c&(1<<25)) strcat(p,"AES ");
        if(maxleaf>=7){ cpuid(7,0,&a,&b,&c,&d); if(b&(1<<5)) strcat(p,"AVX2 "); if(b&(1<<3)) strcat(p,"BMI1 "); }
        if(!p[0]) strcpy(p,"base");
    }
    /* MHz: TSC su 100ms esatti di PIT ch0 in POLLING (funziona anche
       con interrupt disabilitati, pre-sti). Conta 119318 tick PIT
       (=100ms), ignorando glitch di latch >1000. */
    {
        u32 elapsed=0, last=pit_count(), guard=0;
        u64 t0=rdtsc();
        while(elapsed<119318 && guard++<30000000){
            u32 now=pit_count();
            u32 d=(last-now)&0xFFFF;
            if(d<1000) elapsed+=d;
            last=now;
        }
        u64 s1=rdtsc();
        u32 diff32=(u32)(s1-t0);
        mhz=diff32/100000;
        if(mhz<10||mhz>10000) mhz=0;
    }
    /* abilita FPU/SSE */
    if(has_sse){
        u32 cr0,cr4;
        __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
        cr0&=~(1u<<2); cr0|=(1u<<1);
        __asm__ volatile("mov %0, %%cr0"::"r"(cr0));
        __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
        cr4|=(1u<<9)|(1u<<10);
        __asm__ volatile("mov %0, %%cr4"::"r"(cr4));
        sse_on=1;
    }
    /* PAT: PA1 = write-combining per il framebuffer */
    if(has_pat){
        u64 pat=rdmsr(0x277);
        pat=(pat&~0xFF00ULL)|0x0100ULL;
        wrmsr(0x277,pat);
        pat_wc_on=1;
    }
    kprintf("[CPU] %s | %s fam=%d mod=%x step=%d cores=%d\n", vendor_str, brand_str, family, model, stepping, cores);
    kprintf("[CPU] %d MHz misurati", mhz);
    if(base_mhz) kprintf(" base=%d max=%d (leaf16)", base_mhz, max_mhz);
    kprintf(" L1=%dK L2=%dK L3=%dK\n", l1kb, l2kb, l3kb);
    kprintf("[CPU] feat: %s APIC=%d invTSC=%d SSE=%s PAT-WC=%s\n",
        feat_str, has_apic, has_invtcs, sse_on?"ON":"n/d", pat_wc_on?"ON":"n/d");
    if(cpu_is_i3_1115G4())
        kprintf("[CPU] Intel Core i3-1115G4 RILEVATA (Tiger Lake-UP3, pieno supporto)\n");
    else if(model==0x8C&&family==6)
        kprintf("[CPU] Tiger Lake (famiglia i3-1115G4) rilevata\n");
    else
        kprintf("[CPU] profilo i3-1115G4 pronto per hardware reale\n");
}

const char* cpu_vendor(void){ return vendor_str; }
const char* cpu_brand(void){ return brand_str; }
const char* cpu_feats(void){ return feat_str; }
u32 cpu_family(void){ return family; }
u32 cpu_model(void){ return model; }
u32 cpu_stepping(void){ return stepping; }
u32 cpu_mhz(void){ return mhz; }
u32 cpu_base_mhz(void){ return base_mhz; }
u32 cpu_max_mhz(void){ return max_mhz; }
int cpu_cores(void){ return cores; }
int cpu_has_pat_wc(void){ return pat_wc_on; }
void cpu_cache_str(char* buf, size_t n){ ksnprintf(buf,n,"L1 %dK / L2 %dK / L3 %dK", l1kb, l2kb, l3kb); }
int cpu_is_i3_1115G4(void){
    if(!has_cpuid) return 0;
    if(strcmp(vendor_str,"GenuineIntel")!=0) return 0;
    if(family!=6||model!=0x8C) return 0;
    if(strstr(brand_str,"1115G4")) return 1;
    return 0;
}
const char* cpu_short(void){
    if(cpu_is_i3_1115G4()) return "Intel Core i3-1115G4";
    if(family==6&&model==0x8C) return "Intel Tiger Lake (i3-1115G4 family)";
    return brand_str;
}
