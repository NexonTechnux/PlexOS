#include "plexos.h"

/* PMM con bitmap: gestisce fino a 4GB, frame 4KB */
#define MAX_FRAMES (1024*1024)  /* 4GB / 4KB */
static u32* frames_bitmap = NULL;
static u32 total_frames = 0;
static u32 free_frames = 0;
static u32 total_kb = 0;
/* area bitmap statica: 128KB per 1M frame (1 bit per frame) */
static u8 bitmap_static[131072];
static int bitmap_ready = 0;

#define BITMAP_SET(i)   (bitmap_static[(i)/8] |= (1<<((i)%8)))
#define BITMAP_CLEAR(i) (bitmap_static[(i)/8] &= ~(1<<((i)%8)))
#define BITMAP_TEST(i)  (bitmap_static[(i)/8] & (1<<((i)%8)))

extern u32 _kernel_end_addr(void); /* non usato, calcoliamo via linker */
extern char _end[];

void pmm_init(multiboot_info_t* mbi){
    memset(bitmap_static, 0xFF, sizeof(bitmap_static)); /* tutto usato */
    total_kb = mbi->mem_lower + mbi->mem_upper;
    if(total_kb==0) total_kb = 128*1024;
    if(total_kb > 4*1024*1024) total_kb = 4*1024*1024; /* cap 4GB (u32) */
    total_frames = (total_kb*1024)/4096;
    if(total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;
    /* marca liberi i frame sopra 4MB fino a mem totale (lasciamo 0-4MB riservati kernel) */
    u32 first_free = (4*1024*1024)/4096; /* 1024 */
    free_frames = 0;
    for(u32 i=0;i<total_frames;i++){
        if(i>=first_free) { BITMAP_CLEAR(i); free_frames++; }
        else BITMAP_SET(i);
    }
    /* rispetta mmap: marca buchi riservati come usati; se la mappa dice
       piu' RAM di mem_lower/upper (tipico su UEFI), allarga il totale */
    if((mbi->flags>>6)&1){
        u32 addr = mbi->mmap_addr;
        u32 end = addr + mbi->mmap_length;
        u64 maxram = 0;
        u64 iter = 0;
        u32 nent = 0;
        while(addr < end && nent++ < 512){
            mmap_entry_t* e = (mmap_entry_t*)addr;
            if(e->size > 128) break; /* voce corrotta: esci */
            if(e->type != 1){
                u64 s = e->addr, l = e->len;
                if(l > 0x100000000ULL) l = 0x100000000ULL; /* cap 4GB */
                for(u64 a=s; a<s+l; a+=4096){
                    if(++iter > (u64)MAX_FRAMES+64) break;
                    u32 f = (u32)(a/4096);
                    if(f<total_frames && !BITMAP_TEST(f)){ BITMAP_SET(f); if(free_frames)free_frames--; }
                }
            } else {
                if(e->addr+e->len > maxram) maxram = e->addr+e->len;
            }
            addr += e->size + 4;
        }
        if(maxram > (u64)total_kb*1024u){
            u32 kb = maxram>0xFFFFFFFFull ? 0x3FFFFFu : (u32)(maxram/1024u);
            if(kb>total_kb && kb<=4*1024*1024){
                total_kb = kb;
                u32 tf = (total_kb*1024)/4096;
                if(tf>MAX_FRAMES) tf=MAX_FRAMES;
                for(u32 i=total_frames;i<tf;i++){ BITMAP_CLEAR(i); free_frames++; }
                total_frames = tf;
                kprintf("[PMM] totale esteso da mmap: %d KB\n", total_kb);
            }
        }
    }
    bitmap_ready = 1;
    kprintf("[PMM] RAM totale=%d KB, frame liberi=%d\n", total_kb, free_frames);
}

u32 pmm_total_kb(void){ return total_kb; }
u32 pmm_free_kb(void){ return free_frames*4; }

u32 pmm_alloc_frame(void){
    if(!bitmap_ready) return 0;
    for(u32 i=0;i<total_frames;i++){
        if(!BITMAP_TEST(i)){ BITMAP_SET(i); free_frames--; return i*4096; }
    }
    return 0;
}
void pmm_free_frame(u32 addr){
    u32 f = addr/4096;
    if(f<total_frames && BITMAP_TEST(f)){ BITMAP_CLEAR(f); free_frames++; }
}
u32 mem_total_kb(void){ return total_kb; }
