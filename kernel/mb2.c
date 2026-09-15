#include "plexos.h"

/* Entry Multiboot2 (GRUB EFI su PC UEFI): traduce le info MB2 nel formato
   Multiboot1 interno e chiama kmain. Così il resto del kernel resta uguale
   su BIOS e su UEFI (QEMU OVMF e hardware reale i3-1115G4).
   Diagnostica cieca per HW reale: schermi colorati per stadio (foto!). */

static multiboot_info_t mb1;
static mmap_entry_t mb_mmap[40];

/* framebuffer grezzo per marker (indirizzo fisico, vale pre/post paging identity) */
static u64 dbg_fb=0;
static u32 dbg_pitch=0, dbg_w=0, dbg_h=0;

/* Blink CapsLock via i8042: diagnostica senza video. Tutto bounded. */
static void kbd_ww(void){ for(int i=0;i<100000;i++){ if(!(inb(0x64)&2)) return; } }
static int kbd_ack(void){
    for(int i=0;i<100000;i++){
        if(inb(0x64)&1){
            u8 r=inb(0x60);
            if(r==0xFA) return 1;
            if(r==0xFE) return 0;
        }
    }
    return 0;
}
static void kbd_leds(u8 m){
    kbd_ww(); outb(0x60,0xED);
    if(!kbd_ack()) return;
    kbd_ww(); outb(0x60,m);
    kbd_ack();
}
static void led_wait(void){ for(volatile u32 d=0;d<900000;d++); }

/* n lampeggi CapsLock = stadio n. Legenda:
   1=ingresso kernel  2=info MB2 lette  3=IDT/memoria  4=paging+CPU
   5=video pronto  6=sistema pronto (prima della GUI) */
void led_blink(int n){
    for(int i=0;i<n;i++){
        kbd_leds(0x04); led_wait();
        kbd_leds(0x00); led_wait();
    }
    led_wait();
}

static int dbg_ok(void){
    if(dbg_fb==0||dbg_fb>=0x100000000ULL) return 0;
    if(dbg_w<100||dbg_w>4096||dbg_h<100||dbg_h>2160) return 0;
    if(dbg_pitch<dbg_w*4u||dbg_pitch>65536u) return 0;
    return 1;
}

void dbg_set_fb(u64 addr, u32 pitch, u32 w, u32 h){
    dbg_fb=addr; dbg_pitch=pitch; dbg_w=w; dbg_h=h;
}

/* Schermo pieno di un colore = "sono arrivato qui". Legenda:
   MAGENTA=ingresso MB2  BLU=kmain+IDT  ARANCIO=paging ok
   GIALLO=cpu/pci ok  VERDE=fb pronto  ROSSO=panic */
void dbg_stage(u32 color){
    if(!dbg_ok()) return;
    u32 base=(u32)dbg_fb;
    for(u32 y=0;y<dbg_h;y++){
        volatile u32* row=(volatile u32*)(base+y*dbg_pitch);
        for(u32 x=0;x<dbg_w;x++) row[x]=color;
    }
}

void mb2_main(u32 magic, u32 addr){
    serial_init();
    serial_write("[MB2] boot via GRUB multiboot2 (UEFI)\n");
    UNUSED(magic);
    led_blink(1); /* 1: kernel vivo, ingresso MB2 */
    memset(&mb1, 0, sizeof(mb1));
    memset(mb_mmap, 0, sizeof(mb_mmap));

    u32 total = *(volatile u32*)addr;
    if(total>65536) total=65536;
    u32 p = addr + 8;
    u32 end = addr + total;
    int nmap = 0;

    while(p + 8 <= end && p >= addr){
        u32 type = *(volatile u32*)p;
        u32 size = *(volatile u32*)(p + 4);
        if(type == 0 || size < 8 || size > 65536) break;
        if(type == 4 && size >= 16){
            mb1.mem_lower = *(volatile u32*)(p + 8);
            mb1.mem_upper = *(volatile u32*)(p + 12);
            mb1.flags |= 1;
        } else if(type == 6 && size >= 16){
            u32 es = *(volatile u32*)(p + 8);
            if(es < 24) es = 24;
            if(es > 64) es = 64;
            u32 e = p + 16;
            while(e + 24 <= p + size && nmap < 40){
                u64 base = *(volatile u64*)e;
                u64 len  = *(volatile u64*)(e + 8);
                u32 t    = *(volatile u32*)(e + 16);
                if(len > 0x100000000ULL) len = 0x100000000ULL; /* cap 4GB */
                mb_mmap[nmap].size = 20;
                mb_mmap[nmap].addr = base;
                mb_mmap[nmap].len  = len;
                mb_mmap[nmap].type = (t == 1) ? 1 : 2;
                nmap++;
                e += es;
            }
            if(nmap){
                mb1.mmap_addr = (u32)mb_mmap;
                mb1.mmap_length = (u32)(nmap * sizeof(mmap_entry_t));
                mb1.flags |= (1 << 6);
            }
        } else if(type == 8 && size >= 32){
            u64 faddr = *(volatile u64*)(p + 8);
            mb1.fb_addr_lo = (u32)faddr;
            mb1.fb_addr_hi = (u32)(faddr >> 32);
            mb1.fb_pitch  = *(volatile u32*)(p + 16);
            mb1.fb_width  = *(volatile u32*)(p + 20);
            mb1.fb_height = *(volatile u32*)(p + 24);
            mb1.fb_bpp    = *(volatile u8*)(p + 28);
            mb1.fb_type   = *(volatile u8*)(p + 29);
            if(mb1.fb_bpp == 32){ mb1.fb_type = 1; mb1.flags |= (1 << 12); }
            dbg_set_fb(faddr, mb1.fb_pitch, mb1.fb_width, mb1.fb_height);
        }
        p += (size + 7) & ~7u;
    }

    dbg_stage(0xFF00FF); /* MAGENTA: GRUB e' saltato qui, info lette */
    led_blink(2); /* 2: parse info ok */
    kprintf("[MB2] mem=%d+%dK mmap=%d fb=%dx%dx%d%s\n",
        mb1.mem_lower, mb1.mem_upper, nmap,
        mb1.fb_width, mb1.fb_height, mb1.fb_bpp,
        (mb1.fb_addr_hi?" HI!":""));

    kmain(0x2BADB002, &mb1);
    for(;;){ cli(); hlt(); }
}
