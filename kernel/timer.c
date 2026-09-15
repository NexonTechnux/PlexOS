#include "plexos.h"

static volatile u32 ticks = 0;
static u32 freq_hz = 100;

static void timer_cb(regs_t* r){ UNUSED(r); ticks++; }

void timer_init(u32 freq){
    freq_hz = freq ? freq : 100;
    u32 div = 1193180 / freq_hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div>>8) & 0xFF);
    register_interrupt_handler(32, timer_cb);
    kprintf("[TIMER] PIT %d Hz\n", freq_hz);
}
u32 timer_ticks(void){ return ticks; }
u32 uptime_sec(void){ return ticks / freq_hz; }
void sleep_ticks(u32 t){ u32 s=ticks; while(ticks-s<t) hlt(); }
void sleep_ms(u32 ms){ sleep_ticks((ms*freq_hz)/1000); }
