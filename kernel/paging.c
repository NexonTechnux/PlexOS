#include "plexos.h"

/* Paging identity 4GB con pagine 4MB (PSE). Su HW reale verifica PSE via
   CPUID; senza PSE resta in real-mode mapping (QEMU/i3 lo hanno sempre). */

static u32 pdir_4mb[1024] __attribute__((aligned(4096)));
static int paging_on=0;

static inline void cpuid_check(u32 leaf, u32* c, u32* d){
    u32 a,b; __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}

void paging_init(void){
    u32 c=0,d=0;
    cpuid_check(1,&c,&d);
    int pse=(d&(1<<3))!=0;
    for(int i=0;i<1024;i++)
        pdir_4mb[i] = ((u32)i*0x400000) | 0x83; /* present, rw, ps, 4MB */
    __asm__ volatile("mov %0, %%cr3"::"r"(pdir_4mb));
    if(pse){
        u32 cr4; __asm__ volatile("mov %%cr4,%0":"=r"(cr4));
        cr4 |= 0x10; __asm__ volatile("mov %0, %%cr4"::"r"(cr4));
    }
    u32 cr0; __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    cr0 |= 0x80000000; __asm__ volatile("mov %0, %%cr0"::"r"(cr0));
    paging_on=1;
    kprintf("[PAGING] abilitata (%s, 4GB identity)\n", pse?"PSE 4MB":"4KB-compat");
}

/* Marca un range fisico write-combining (PWT) per framebuffer veloci.
   Richiede PAT PA1=WC (impostata da cpu_init). Ricarica CR3 = flush TLB. */
void paging_mark_wc(u32 phys, u32 len){
    if(!paging_on||len==0) return;
    u32 start=phys&~0x3FFFFFu;
    u32 end=(phys+len+0x3FFFFFu)&~0x3FFFFFu;
    for(u32 a=start;a<end;a+=0x400000){
        u32 idx=a>>22;
        if(idx<1024) pdir_4mb[idx]|=(1u<<3); /* PWT -> indice PAT 1 = WC */
    }
    u32 cr3; __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
    __asm__ volatile("mov %0, %%cr3"::"r"(cr3));
    kprintf("[PAGING] WC su %x+%x (framebuffer veloce)\n", phys, len);
}
