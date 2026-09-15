#include "plexos.h"

void sys_reboot(void){
    serial_write("REBOOT da GUI\n");
    outb(0x64,0xFE);
    sleep_ms(200);
    cli();
    static struct { u16 l; u32 b; } __attribute__((packed)) z={0,0};
    __asm__ volatile("lidt %0"::"m"(z));
    __asm__ volatile("int $3");
    for(;;){ cli(); hlt(); }
}
void sys_poweroff(void){
    serial_write("SHUTDOWN da GUI\n");
    outw(0x604,0x2000);  /* q35 */
    outw(0xB004,0x2000); /* bochs/isa-debug-exit */
    outb(0x8900,0);
    for(;;){ cli(); hlt(); }
}
