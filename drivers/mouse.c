#include "plexos.h"

/* Mouse PS/2 QEMU */

static int mx=640, my=360, mbtn=0, moved_flag=0;
static u8 cycle=0; static s8 b[3];

static void mouse_wait(int w){
    int t=100000;
    while(t--){
        if(w){ if(inb(0x64)&2) continue; return; }
        else { if(inb(0x64)&1) return; }
    }
}
static void mouse_write(u8 v){ mouse_wait(1); outb(0x64,0xD4); mouse_wait(1); outb(0x60,v); }
static u8 mouse_read(void){ mouse_wait(0); return inb(0x60); }

void mouse_handler(regs_t* r){
    UNUSED(r);
    u8 s = inb(0x64);
    if(!(s&0x20)) return;
    u8 d = inb(0x60);
    if(cycle==0){ if(!(d&0x08)) return; b[0]=d; cycle=1; }
    else if(cycle==1){ b[1]=(s8)d; cycle=2; }
    else {
        b[2]=(s8)d; cycle=0;
        int dx=b[1], dy=-b[2];
        mx+=dx; my+=dy;
        int W=(int)fb_width(), H=(int)fb_height();
        if(mx<0)mx=0; if(my<0)my=0; if(mx>=W)mx=W-1; if(my>=H)my=H-1;
        mbtn = b[0]&0x7;
        moved_flag=1;
    }
}

void mouse_init(void){
    mx=fb_width()/2; my=fb_height()/2;
    mouse_wait(1); outb(0x64,0xA8);
    mouse_wait(1); outb(0x64,0x20);
    mouse_wait(0); u8 st=mouse_read();
    mouse_wait(1); outb(0x64,0x60);
    mouse_wait(1); outb(0x60,st|2);
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
    u8 m=inb(0xA1); outb(0xA1, m & ~(1<<4)); /* sblocca IRQ12 */
    m=inb(0x21); outb(0x21, m & ~(1<<2));    /* sblocca cascade */
    register_interrupt_handler(44, mouse_handler);
    kprintf("[MOUSE] PS/2 inizializzato\n");
}

int mouse_x(void){ return mx; }
int mouse_y(void){ return my; }
int mouse_buttons(void){ return mbtn; }
int mouse_moved(void){ int v=moved_flag; moved_flag=0; return v; }
