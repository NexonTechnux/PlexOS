#include "plexos.h"

/* ATA PIO per QEMU (ide-hd). Usa primary master. */
#define ATA_D0 0x1F0
#define ATA_ERR 0x1F1
#define ATA_SEC 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRV 0x1F6
#define ATA_STA 0x1F7
#define ATA_CMD 0x1F7
#define ATA_CTL 0x3F6

static int present=0;
static u32 total_sectors=0;

static void ata_wait(void){
    for(int i=0;i<4;i++) inb(ATA_STA);
}
static int ata_wait_ready(void){
    int t=100000;
    while(t--){
        u8 s=inb(ATA_STA);
        if(!(s&0x80) && (s&0x40)) return 0;
        if(s&1) return -1;
    }
    return -1;
}

void ata_init(void){
    /* floating bus? */
    u8 s=inb(ATA_STA);
    if(s==0xFF){ kprintf("[ATA] nessun controller IDE\n"); present=0; return; }
    outb(ATA_DRV, 0xE0); ata_wait();
    outb(ATA_SEC,0); outb(ATA_LBA0,0); outb(ATA_LBA1,0); outb(ATA_LBA2,0);
    outb(ATA_CMD,0xEC); /* IDENTIFY */
    s=inb(ATA_STA);
    if(s==0){ kprintf("[ATA] nessun disco\n"); present=0; return; }
    if(ata_wait_ready()!=0){ kprintf("[ATA] disco non pronto\n"); present=0; return; }
    u16 id[256];
    for(int i=0;i<256;i++) id[i]=inw(ATA_D0);
    total_sectors = ((u32)id[61]<<16)|id[60];
    if(total_sectors==0) total_sectors=131072;
    present=1;
    kprintf("[ATA] QEMU HDD rilevato: %d settori (%d MB)\n", total_sectors, total_sectors/2048);
}

int ata_present(void){ return present; }
int ata_sectors(void){ return (int)total_sectors; }

static int ata_pio_rw(u32 lba, u8* buf, u32 sectors, int write){
    if(!present) return -1;
    for(u32 i=0;i<sectors;i++){
        ata_wait();
        outb(ATA_DRV, 0xE0 | ((lba>>24)&0x0F));
        outb(ATA_SEC, 1);
        outb(ATA_LBA0, lba&0xFF);
        outb(ATA_LBA1, (lba>>8)&0xFF);
        outb(ATA_LBA2, (lba>>16)&0xFF);
        outb(ATA_CMD, write?0x30:0x20);
        if(ata_wait_ready()!=0) return -1;
        if(write){
            for(int w=0;w<256;w++){ outw(ATA_D0, ((u16*)buf)[w]); }
            /* flush */
            outb(ATA_CMD, 0xE7);
            ata_wait();
        } else {
            for(int w=0;w<256;w++){ ((u16*)buf)[w]=inw(ATA_D0); }
        }
        lba++; buf+=512;
    }
    return 0;
}
int ata_read(u32 lba, u8* buf, u32 sectors){ return ata_pio_rw(lba,buf,sectors,0); }
int ata_write(u32 lba, const u8* buf, u32 sectors){ return ata_pio_rw(lba,(u8*)buf,sectors,1); }
