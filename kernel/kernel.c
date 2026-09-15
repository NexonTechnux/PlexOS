#include "plexos.h"

void panic(const char* msg){
    cli();
    serial_write("PANIC: "); serial_write(msg); serial_write("\n");
    dbg_stage(0xFF0000); /* ROSSO fisso: panic anche senza font */
    if(fb_ready()){
        fb_clear(rgb(180,0,0));
        fb_draw_string(40,40,"*** KERNEL PANIC ***",rgb(255,255,255));
        fb_draw_string(40,70,msg,rgb(255,255,0));
        fb_draw_string(40,100,"Sistema arrestato.",rgb(255,255,255));
        fb_vsync_wait();
        fb_present();
    }
    for(;;){ cli(); hlt(); }
}

void kmain(u32 magic, multiboot_info_t* mbi){
    serial_init();
    serial_write("\n==============================\n PlexOS 1.0 - boot (NexonTech)\n==============================\n");

    if(magic!=0x2BADB002){
        serial_write("Bad multiboot magic!\n");
        /* prova comunque */
    }
    kprintf("[BOOT] magic=%x mbi=%x\n", magic, (u32)mbi);

    /* path BIOS: geometria fb per marker diagnostici */
    if(magic==0x2BADB002 && ((mbi->flags>>12)&1) && mbi->fb_bpp==32)
        dbg_set_fb(((u64)mbi->fb_addr_hi<<32)|mbi->fb_addr_lo,
            mbi->fb_pitch, mbi->fb_width, mbi->fb_height);

    gdt_init();
    idt_init();
    sti(); /* IDT+PIC pronti: abilita subito (sleep/hlt sicuri ovunque) */
    dbg_stage(0x0000FF); /* BLU: base ok */
    led_blink(3); /* 3: base+IDT ok */
    pmm_init(mbi);
    paging_init();
    dbg_stage(0xFF8000); /* ARANCIO: memoria ok */
    timer_init(100);
    cpu_init();   /* prima del fb: SSE/PAT pronti per framebuffer veloce */
    pci_init();   /* prima del fb: serve a Bochs/GPU per BAR e detect */
    led_blink(4); /* 4: memoria+CPU+PCI ok */

    if(fb_init(mbi)!=0){
        kprintf("[BOOT] continuo in VGA-text (no GUI accel)\n");
        fb_dev_notice(); /* schermata rossa: display assente = STILL IN DEVELOPMENT */
    }
    dbg_stage(0xFFFF00); /* GIALLO: cpu/pci/fb ok */
    led_blink(5); /* 5: video ok */
    keyboard_init();
    /* mouse dopo fb per centratura */
    mouse_init();
    gpu_init();
    ata_init();
    net_init();
    fs_init();

    kprintf("[BOOT] PlexOS pronto: %dx%d 60fps | %s | %s | NET %s\n",
        fb_width(), fb_height(), cpu_short(), gpu_name(), net_name());
    led_blink(6); /* 6: sistema pronto, parte la GUI */

    sti();

    /* splash 720p (double-buffered: disegna + present + vsync) */
    if(fb_ready()){
        fb_clear(rgb(14,18,30));
        int W=fb_width(), H=fb_height();
        fb_fill_circle(W/2,H/2-40,70,rgb(0,180,255));
        fb_fill_circle(W/2,H/2-40,55,rgb(14,18,30));
        fb_draw_string(W/2-120,H/2+50,"PlexOS 1.0 - Caricamento... 60fps",rgb(255,255,255));
        fb_draw_string(W/2-140,H/2+70,"Kernel 720p + QEMU/HW reale - NexonTech",rgb(120,180,220));
        fb_vsync_wait(); fb_present();
        /* barra caricamento */
        for(int i=0;i<=100;i+=5){
            fb_fill_rect(W/2-150,H/2+100,300,18,rgb(40,50,70));
            fb_fill_rect(W/2-150,H/2+100,(300*i)/100,18,rgb(0,180,255));
            fb_vsync_wait(); fb_present();
            sleep_ms(60);
        }
    } else {
        sleep_ms(500);
    }

    gui_init();
    gui_run();

    panic("gui_run ritornata!");
}
