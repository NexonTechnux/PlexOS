#include "plexos.h"

/* GPU QEMU: Bochs Display Interface (VBE dispi) su VGA std (1234:1111).
   Imposta il modo video DIRETTAMENTE via porte, senza dipendere dal VBE
   di GRUB/SeaBIOS (che si e' mostrato inaffidabile). Su HW reale senza
   Bochs e' inerte: si usa il framebuffer GOP/VBE di GRUB. */

#define DISPI_I 0x1CE
#define DISPI_D 0x1CF
#define DISPI_ID0 0
#define DISPI_XRES 1
#define DISPI_YRES 2
#define DISPI_BPP 3
#define DISPI_ENABLE 4
#define DISPI_BANK 5
#define DISPI_VIRTW 6
#define DISPI_VIRTH 7
#define DISPI_XOFF 8
#define DISPI_YOFF 9

#define DISPI_DISABLED 0x00
#define DISPI_ENABLED  0x01
#define DISPI_LFB      0x40

static u16 dr(u16 i){ outw(DISPI_I,i); return inw(DISPI_D); }
static void dw(u16 i,u16 v){ outw(DISPI_I,i); outw(DISPI_D,v); }

int bochs_present(void){
    u16 id=dr(DISPI_ID0);
    return id>=0xB0C0&&id<=0xB0C6;
}

u32 bochs_lfb(void){
    pci_dev_t* v=pci_find(0x1234,0x1111);
    if(v){
        u32 bar=pci_bar_addr(v,0);
        if(bar&&!(bar&1)) return bar;
    }
    return 0;
}

/* Imposta modo grafico, ritorna indirizzo fisico LFB o 0 se impossibile.
   Richiede VRAM sufficiente (QEMU std default 16MB: 720p=3.7MB, 1080p=8.3MB). */
u32 bochs_set_mode(u32 w, u32 h, u32 bpp){
    if(!bochs_present()) return 0;
    if(bpp!=32) return 0;
    if(w==0||h==0||w>1920||h>1080) return 0;
    u32 lfb=bochs_lfb();
    if(!lfb) return 0;
    if((u64)w*h*4 > 16u*1024u*1024u) return 0;
    dw(DISPI_ENABLE, DISPI_DISABLED);
    dw(DISPI_XRES,(u16)w); dw(DISPI_YRES,(u16)h);
    dw(DISPI_BPP,32);
    dw(DISPI_VIRTW,(u16)w); dw(DISPI_VIRTH,(u16)h);
    dw(DISPI_BANK,0); dw(DISPI_XOFF,0); dw(DISPI_YOFF,0);
    dw(DISPI_ENABLE, DISPI_ENABLED|DISPI_LFB);
    kprintf("[BOCHS] modo %dx%dx32 LFB=%x\n", w, h, lfb);
    return lfb;
}
