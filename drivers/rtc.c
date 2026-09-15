#include "plexos.h"

static u8 cmos(int r){
    outb(0x70, r);
    return inb(0x71);
}
static int bcd(int v){ return (v&0x0F)+((v>>4)*10); }

void rtc_read(rtc_time_t* t){
    int tries=0;
    while((cmos(0x0A)&0x80)&&tries++<100000);
    t->sec=cmos(0x00); t->min=cmos(0x02); t->hour=cmos(0x04);
    t->day=cmos(0x07); t->mon=cmos(0x08); t->year=cmos(0x09);
    u8 b=cmos(0x0B);
    if(!(b&0x04)){ t->sec=bcd(t->sec); t->min=bcd(t->min); t->hour=bcd(t->hour&0x7F); t->day=bcd(t->day); t->mon=bcd(t->mon); t->year=bcd(t->year); }
    t->year+=2000;
    if(t->year<2020||t->year>2100){ t->day=1;t->mon=1;t->year=2026;t->hour=12;t->min=0;t->sec=0; }
}
void rtc_format(char* buf, rtc_time_t* t){
    ksnprintf(buf,32,"%02d/%02d/%04d %02d:%02d:%02d", t->day,t->mon,t->year,t->hour,t->min,t->sec);
}
