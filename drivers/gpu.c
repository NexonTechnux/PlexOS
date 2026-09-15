#include "plexos.h"

/* GPU: Intel UHD Graphics G4 (Tiger Lake GT2) + Bochs/QEMU.
   Su HW reale il framebuffer Multiboot/VBE-GOP resta valido. */

static char name[64]="Sconosciuta";
static char meminfo[64]="";
static int is_intel=0, is_uhd=0;

static u32 bar_size(u8 bus, u8 slot, u8 func, int idx){
    u8 off=0x10+idx*4;
    u32 orig=pci_cfg_read(bus,slot,func,off);
    pci_cfg_write(bus,slot,func,off,0xFFFFFFFFu);
    u32 sz=pci_cfg_read(bus,slot,func,off);
    pci_cfg_write(bus,slot,func,off,orig);
    if(sz==0||sz==0xFFFFFFFFu) return 0;
    if(sz&1) sz&=0xFFFFFFFCu; else sz&=0xFFFFFFF0u;
    return (~sz)+1;
}

void gpu_init(void){
    strcpy(name,"Sconosciuta");
    is_intel=0; is_uhd=0;
    for(int i=0;i<pci_count();i++){
        pci_dev_t* d=pci_get(i);
        if(d->vendor==0x8086&&d->class_==0x03){
            is_intel=1;
            switch(d->device){
                case 0x9A78: strcpy(name,"Intel UHD Graphics G4 (TGL GT2 9A78)"); is_uhd=1; break;
                case 0x9A49: strcpy(name,"Intel UHD Graphics G4 (TGL 9A49)"); is_uhd=1; break;
                case 0x9A40: strcpy(name,"Intel UHD Graphics (TGL 9A40)"); is_uhd=1; break;
                case 0x9A59: strcpy(name,"Intel UHD Graphics (TGL 9A59)"); is_uhd=1; break;
                case 0x9BC4: strcpy(name,"Intel UHD Graphics (Rocket Lake)"); break;
                case 0x5912: strcpy(name,"Intel HD Graphics 630"); break;
                default: ksnprintf(name,sizeof(name),"Intel VGA %04x", d->device); break;
            }
            kprintf("[GPU] %s @ %02x:%02x.%d\n", name, d->bus,d->slot,d->func);
            {
                u32 b0=bar_size(d->bus,d->slot,d->func,0);
                u32 b2=bar_size(d->bus,d->slot,d->func,2);
                ksnprintf(meminfo,sizeof(meminfo),"MMIO %dMB + Aperture %dMB",
                    b0/1048576, b2/1048576);
                kprintf("[GPU] %s\n", meminfo);
            }
            return;
        }
    }
    /* fallback QEMU Bochs */
    pci_dev_t* b=pci_find(0x1234,0x1111);
    if(b){ strcpy(name,"Bochs/QEMU VGA std (720p)"); kprintf("[GPU] %s\n", name); return; }
    /* Cirrus o altro VGA QEMU */
    pci_dev_t* v=pci_find_class(0x03,0xFF);
    if(v){ ksnprintf(name,sizeof(name),"VGA %04x:%04x (QEMU)", v->vendor, v->device); kprintf("[GPU] %s\n", name); return; }
    kprintf("[GPU] nessuna GPU PCI, uso framebuffer Multiboot\n");
    strcpy(name,"Framebuffer VBE/GOP generico");
}

const char* gpu_name(void){ return name; }
const char* gpu_mem(void){ return meminfo; }
int gpu_is_intel(void){ return is_intel; }
int gpu_is_uhd_g4(void){ return is_uhd; }
