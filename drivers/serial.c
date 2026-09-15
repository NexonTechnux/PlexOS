#include "plexos.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x80);
    outb(COM1+0, 0x03);
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x03);
    outb(COM1+2, 0xC7);
    outb(COM1+4, 0x0B);
    /* QEMU debug port 0xE9 abilitata implicitamente */
}
static int serial_ready(void){ return inb(COM1+5)&0x20; }

/* ring buffer diagnostico per `dmesg` */
#define LOGSZ 8192
static char logring[LOGSZ];
static u32 logpos=0;

void dmesg_dump(char* out, size_t outsz){
    if(outsz==0) return;
    u32 total=logpos<LOGSZ?logpos:LOGSZ;
    u32 start=(logpos<LOGSZ)?0:(logpos%LOGSZ);
    size_t n=total;
    if(n>outsz-1) n=outsz-1;
    /* ultimi n caratteri */
    u32 from=(start+total-n)%LOGSZ;
    for(size_t i=0;i<n;i++) out[i]=logring[(from+i)%LOGSZ];
    out[n]=0;
}

void serial_putc(char c){
    logring[logpos%LOGSZ]=c; logpos++;
    if(c=='\n'){ while(!serial_ready()); outb(COM1,'\r'); }
    while(!serial_ready()); outb(COM1,c);
    /* echo anche su porta debug Bochs/QEMU */
    outb(0xE9, c);
}
void serial_write(const char* s){ while(*s) serial_putc(*s++); }
void serial_write_hex(u32 n){
    char b[16]; utoa(n,b,16); serial_write("0x"); serial_write(b);
}
void serial_write_dec(u32 n){
    char b[16]; utoa(n,b,10); serial_write(b);
}
