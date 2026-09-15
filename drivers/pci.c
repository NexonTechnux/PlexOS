#include "plexos.h"

/* PCI: enumerazione generica (QEMU + hardware reale).
   Bus 0-7 per coprire chipset Intel reali (i3-1115G4) senza rallentare troppo. */
#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static pci_dev_t devs[64];
static int ndevs=0;

u32 pci_cfg_read(u8 bus,u8 slot,u8 func,u8 off){
    u32 a=(1u<<31)|((u32)bus<<16)|((u32)slot<<11)|((u32)func<<8)|(off&0xFC);
    outl(PCI_ADDR,a);
    return inl(PCI_DATA);
}
void pci_cfg_write(u8 bus,u8 slot,u8 func,u8 off,u32 v){
    u32 a=(1u<<31)|((u32)bus<<16)|((u32)slot<<11)|((u32)func<<8)|(off&0xFC);
    outl(PCI_ADDR,a);
    outl(PCI_DATA,v);
}
u32 pci_bar_addr(pci_dev_t* d, int idx){
    if(!d||idx<0||idx>5) return 0;
    u32 b=pci_cfg_read(d->bus,d->slot,d->func,0x10+idx*4);
    if(b&1) return b&0xFFFFFFFC; /* IO */
    return b&0xFFFFFFF0;         /* MMIO */
}
void pci_enable_mmio_busmaster(pci_dev_t* d){
    u32 c=pci_cfg_read(d->bus,d->slot,d->func,0x04);
    c|=(1<<1)|(1<<2); /* MMIO + bus master */
    pci_cfg_write(d->bus,d->slot,d->func,0x04,c);
}
pci_dev_t* pci_find(u16 vendor, u16 device){
    for(int i=0;i<ndevs;i++)
        if(devs[i].vendor==vendor&&devs[i].device==device) return &devs[i];
    return NULL;
}
pci_dev_t* pci_find_class(u8 class_, u8 subclass){
    for(int i=0;i<ndevs;i++)
        if(devs[i].class_==class_&&(subclass==0xFF||devs[i].subclass==subclass)) return &devs[i];
    return NULL;
}

const char* pci_class_name(u8 c){
    switch(c){
        case 0x00: return "Legacy";
        case 0x01: return "Storage (IDE/SATA)";
        case 0x02: return "Network";
        case 0x03: return "Display (VGA)";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Comm";
        case 0x08: return "System";
        case 0x09: return "Input";
        case 0x0C: return "Serial Bus (USB)";
        default: return "Altro";
    }
}

void pci_init(void){
    ndevs=0;
    for(int bus=0;bus<8;bus++) for(int slot=0;slot<32;slot++){
        u32 v=pci_cfg_read(bus,slot,0,0);
        u16 vendor=v&0xFFFF;
        if(vendor==0xFFFF) continue;
        for(int func=0;func<8;func++){
            u32 v2=pci_cfg_read(bus,slot,func,0);
            if((v2&0xFFFF)==0xFFFF) continue;
            u32 c=pci_cfg_read(bus,slot,func,8);
            u32 irq=pci_cfg_read(bus,slot,func,0x3C);
            if(ndevs<64){
                devs[ndevs].bus=bus; devs[ndevs].slot=slot; devs[ndevs].func=func;
                devs[ndevs].vendor=v2&0xFFFF; devs[ndevs].device=(v2>>16)&0xFFFF;
                devs[ndevs].class_=(c>>24)&0xFF; devs[ndevs].subclass=(c>>16)&0xFF; devs[ndevs].prog=(c>>8)&0xFF;
                devs[ndevs].irq=irq&0xFF;
                ndevs++;
            }
            u32 ht=pci_cfg_read(bus,slot,func,12);
            if(func==0 && !((ht>>16)&0x80)) break;
        }
    }
    kprintf("[PCI] trovati %d dispositivi (bus 0-7, HW reale ready)\n", ndevs);
    for(int i=0;i<ndevs;i++)
        kprintf("  %02x:%02x.%d %04x:%04x %s\n", devs[i].bus,devs[i].slot,devs[i].func,
            devs[i].vendor,devs[i].device, pci_class_name(devs[i].class_));
}
int pci_count(void){ return ndevs; }
pci_dev_t* pci_get(int i){ if(i<0||i>=ndevs) return NULL; return &devs[i]; }
