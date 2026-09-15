#include "plexos.h"

/* Display Intel (Tiger Lake / UHD G4): legge la superficie REALMENTE
   scansionata dal plane 1A. Solo letture; se sana e diversa da quella di
   GRUB, il kernel la adotta (schermo nero = fb sbagliato di GRUB).
   Offsets TGL (gen12): plane1A CTL 0x70880 / SURF 0x7089C,
   legacy primary A CTL 0x70180 / SURF 0x7019C. */

static u32 gpu_mmio=0;
static u32 surf_a=0;
static int plane_on=0;
static int probed=0, ok=0, seen=0;

static u32 R(u32 off){ return *(volatile u32*)(gpu_mmio+off); }

static int sane_addr(u32 a){
    if(a<0x100000u) return 0;          /* sotto 1MB: no */
    if((a&0xFFFu)!=0) return 0;        /* non allineata: no */
    return 1;
}

int intel_disp_probe(void){
    probed=1; ok=0; surf_a=0; plane_on=0; gpu_mmio=0;
    pci_dev_t* g=NULL;
    for(int i=0;i<pci_count();i++){
        pci_dev_t* d=pci_get(i);
        if(d->vendor==0x8086&&d->class_==0x03){ g=d; break; }
    }
    if(!g) return 0;
    seen=1;
    u32 bar=pci_bar_addr(g,0);
    if(!bar||(bar&1u)) return 0;
    gpu_mmio=bar;
    u32 ctl1=R(0x70880), s1=R(0x7089C);
    u32 cleg=R(0x70180), sleg=R(0x7019C);
    if(ctl1==0xFFFFFFFFu&&s1==0xFFFFFFFFu&&cleg==0xFFFFFFFFu&&sleg==0xFFFFFFFFu)
        return 0; /* MMIO non decodificata */
    if(ctl1&0x80000000u){ plane_on=1; if(sane_addr(s1&0xFFFFF000u)) surf_a=s1&0xFFFFF000u; }
    if(!surf_a && (cleg&0x80000000u)){
        plane_on=1;
        if(sane_addr(sleg&0xFFFFF000u)) surf_a=sleg&0xFFFFF000u;
    }
    if(surf_a){ ok=1; return 1; }
    return 0;
}

u32 intel_disp_surf(void){ return probed?surf_a:0; }
int intel_disp_on(void){ return plane_on; }
int intel_disp_seen(void){ return seen; }
