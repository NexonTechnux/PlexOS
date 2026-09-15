#ifndef PLEXOS_H
#define PLEXOS_H

/* ===== Tipi base ===== */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef unsigned int size_t;
typedef unsigned int uintptr_t;

#define NULL ((void*)0)
#define UNUSED(x) (void)(x)

/* ===== I/O porte ===== */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 ret; __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline u16 inw(u16 port) {
    u16 ret; __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline u32 inl(u16 port) {
    u32 ret; __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port)); return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* ===== Serial (debug) ===== */
void serial_init(void);
void serial_putc(char c);
void serial_write(const char* s);
void serial_write_hex(u32 n);
void serial_write_dec(u32 n);
void dmesg_dump(char* out, size_t outsz);

/* ===== Libc ===== */
size_t strlen(const char* s);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* d, const char* s);
char* strncpy(char* d, const char* s, size_t n);
char* strcat(char* d, const char* s);
char* strchr(const char* s, int c);
char* strstr(const char* h, const char* n);
int memcmp(const void* a, const void* b, size_t n);
void* memcpy(void* d, const void* s, size_t n);
void* memmove(void* d, const void* s, size_t n);
void* memset(void* s, int c, size_t n);
int atoi(const char* s);
u32 atou(const char* s);
void itoa(int v, char* buf, int base);
void utoa(u32 v, char* buf, int base);
int toupper_c(int c);
int tolower_c(int c);
int isdigit_c(int c);
int isalpha_c(int c);
int isspace_c(int c);
double atof_simple(const char* s);
void ftoa_simple(double v, char* buf, int prec);

/* printf kernel (su serial + framebuffer console debug) */
int kprintf(const char* fmt, ...);
int ksprintf(char* buf, const char* fmt, ...);
int ksnprintf(char* buf, size_t n, const char* fmt, ...);

/* ===== Multiboot ===== */
typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u32 fb_addr_lo;
    u32 fb_addr_hi;
    u32 fb_pitch;
    u32 fb_width;
    u32 fb_height;
    u8  fb_bpp;
    u8  fb_type;
    u8  fb_reserved[6];
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
} __attribute__((packed)) mmap_entry_t;

/* ===== GDT/IDT ===== */
void gdt_init(void);
void idt_init(void);

typedef struct {
    u32 ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, useresp, ss;
} regs_t;

void isr_handler(regs_t* r);
void irq_handler(regs_t* r);
typedef void (*isr_t)(regs_t*);
void register_interrupt_handler(u8 n, isr_t h);

/* ===== PMM / paging / heap ===== */
void pmm_init(multiboot_info_t* mbi);
u32 pmm_total_kb(void);
u32 pmm_free_kb(void);
u32 pmm_alloc_frame(void);   /* ritorna indirizzo fisico, 0 se OOM */
void pmm_free_frame(u32 addr);
void paging_init(void);
void paging_mark_wc(u32 phys, u32 len);
void* kmalloc(size_t size);
void* kcalloc(size_t n, size_t sz);
void* krealloc(void* p, size_t ns);
void kfree(void* p);
u32 kmalloc_used(void);
char* kstrdup(const char* s);

/* ===== Timer PIT ===== */
void timer_init(u32 freq);
u32 timer_ticks(void);
u32 uptime_sec(void);
void sleep_ms(u32 ms);
void sleep_ticks(u32 t);

/* ===== Framebuffer 720p ===== */
int  fb_init(multiboot_info_t* mbi);
u32  fb_width(void);
u32  fb_height(void);
u32  fb_pitch(void);
u8   fb_bpp(void);
int  fb_ready(void);
void fb_putpixel(int x, int y, u32 color);
u32  fb_getpixel(int x, int y);
void fb_clear(u32 color);
void fb_fill_rect(int x, int y, int w, int h, u32 color);
void fb_draw_rect(int x, int y, int w, int h, u32 color);
void fb_draw_line(int x0,int y0,int x1,int y1,u32 color);
void fb_draw_circle(int cx,int cy,int r,u32 color);
void fb_fill_circle(int cx,int cy,int r,u32 color);
void fb_draw_char(int x,int y,char c,u32 fg,u32 bg,int transparent);
void fb_draw_string(int x,int y,const char* s,u32 color);
void fb_draw_string_bg(int x,int y,const char* s,u32 fg,u32 bg);
int  fb_char_w(void);
int  fb_char_h(void);
u32  rgb(u8 r,u8 g,u8 b);
u8 font_row(char c, int row);
void fb_present(void);      /* copia backbuffer -> video (vsync) */
void fb_vsync_wait(void);   /* attesa retrace verticale QEMU/VGA */
int  fb_double_buffered(void);
const char* fb_source(void); /* "GRUB-VBE", "GRUB-GOP", "BOCHS", "VGA-text" */
void fb_dev_notice(void);    /* schermata rossa "STILL IN DEVELOPMENT" se display assente */

/* GPU QEMU Bochs dispi (modo diretto, no GRUB) */
int bochs_present(void);
u32 bochs_lfb(void);
u32 bochs_set_mode(u32 w, u32 h, u32 bpp);

/* Display Intel reale: superficie scansionata (Tiger Lake) */
int intel_disp_probe(void);
u32 intel_disp_surf(void);
int intel_disp_on(void);
int intel_disp_seen(void);

/* colori tema */
extern u32 TH_BG, TH_BAR, TH_ACCENT, TH_WINBG, TH_TITLE, TH_TEXT, TH_TASKBAR;

/* ===== Tastiera ===== */
void keyboard_init(void);
int  kbd_has_char(void);
char kbd_getchar(void);          /* bloccante-ish: ritorna 0 se vuoto */
int  kbd_has_key(void);          /* tasti speciali disponibili */
int  kbd_getkey(void);           /* ritorna keycode esteso */
void keyboard_handler(regs_t* r);
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103
#define KEY_HOME  0x104
#define KEY_END   0x105
#define KEY_MENU  0x106   /* F10: apre/chiude Plex Start */
#define KEY_ENTER '\n'
#define KEY_BSPACE '\b'
#define KEY_TAB   '\t'
#define KEY_ESC   27

/* ===== Mouse PS/2 ===== */
void mouse_init(void);
void mouse_handler(regs_t* r);
int mouse_x(void);
int mouse_y(void);
int mouse_buttons(void);   /* bit0 sx, bit1 dx, bit2 centro */
int mouse_moved(void);

/* ===== PCI ===== */
typedef struct { u8 bus, slot, func; u16 vendor, device; u8 class_, subclass, prog; u8 irq; } pci_dev_t;
void pci_init(void);
int pci_count(void);
pci_dev_t* pci_get(int i);
const char* pci_class_name(u8 class_);
u32 pci_cfg_read(u8 bus, u8 slot, u8 func, u8 off);
void pci_cfg_write(u8 bus, u8 slot, u8 func, u8 off, u32 v);
u32 pci_bar_addr(pci_dev_t* d, int idx);
void pci_enable_mmio_busmaster(pci_dev_t* d);
pci_dev_t* pci_find(u16 vendor, u16 device);
pci_dev_t* pci_find_class(u8 class_, u8 subclass);

/* ===== CPU (Intel Core i3-1115G4 ready, HW reale) ===== */
void cpu_init(void);
const char* cpu_vendor(void);
const char* cpu_brand(void);
u32 cpu_family(void);
u32 cpu_model(void);
u32 cpu_stepping(void);
u32 cpu_mhz(void);
u32 cpu_base_mhz(void);
u32 cpu_max_mhz(void);
const char* cpu_feats(void);
int cpu_has_pat_wc(void);
void cpu_cache_str(char* buf, size_t n);
int cpu_cores(void);
int cpu_is_i3_1115G4(void);
const char* cpu_short(void);   /* "Intel Core i3-1115G4" o brand QEMU */

/* ===== GPU (Intel UHD Graphics G4 ready, HW reale) ===== */
void gpu_init(void);
const char* gpu_name(void);
const char* gpu_mem(void);
int gpu_is_intel(void);
int gpu_is_uhd_g4(void);

/* ===== Rete Intel generica (e1000 82540EM/82545EM) ===== */
void net_init(void);
int net_present(void);
const char* net_name(void);
u8* net_mac(void);
void net_mac_str(char* buf);
int net_link(void);
int net_enabled(void);
void net_set_enabled(int on);
u32 net_tx_count(void);
u32 net_rx_count(void);
int net_tx_test(void);         /* invia frame broadcast di test */
const char* net_ip(void);
void net_poll(void);

/* ===== ATA ===== */
void ata_init(void);
int ata_present(void);
int ata_sectors(void);
int ata_read(u32 lba, u8* buf, u32 sectors);
int ata_write(u32 lba, const u8* buf, u32 sectors);

/* ===== RTC ===== */
typedef struct { int sec,min,hour,day,mon,year; } rtc_time_t;
void rtc_read(rtc_time_t* t);
void rtc_format(char* buf, rtc_time_t* t);

/* ===== VFS ===== */
typedef struct fs_node fs_node_t;
struct fs_node {
    char name[64];
    int is_dir;
    char* data;
    u32 size;
    u32 cap;
    fs_node_t* parent;
    fs_node_t* child;   /* primo figlio */
    fs_node_t* next;    /* fratello */
};
void fs_init(void);
fs_node_t* fs_root(void);
fs_node_t* fs_resolve(const char* path);   /* supporta assoluti e relativi a cwd */
fs_node_t* fs_resolve_from(fs_node_t* base, const char* path);
int fs_mkdir(const char* path);
int fs_touch(const char* path);
int fs_write_file(const char* path, const char* data, u32 len);
int fs_append_file(const char* path, const char* data, u32 len);
int fs_rm(const char* path);
int fs_is_dir(fs_node_t* n);
void fs_set_cwd(const char* path);
fs_node_t* fs_cwd(void);
void fs_cwd_str(char* buf, size_t n);
int fs_count(void);
int fs_list_names(fs_node_t* dir, char names[][64], int maxn);

/* ===== Shell ===== */
void shell_execute(const char* line, char* out, size_t outsz, int* clear_req, int* exit_req);
void shell_complete_info(void);

/* ===== GUI ===== */
typedef struct gui_window gui_window_t;
struct gui_window {
    int used;
    int id;
    int x,y,w,h;
    int focused;
    int minimized;
    int closed;
    int dragging;
    int drag_ox, drag_oy;
    char title[64];
    int kind; /* 0=terminal 1=fileman 2=calc 3=browser 4=editor 5=impostazioni */
    void* priv;
    int scroll;
    int anim;       /* 0=ferma 1=apertura 2=minimizza 3=ripristino */
    u32 anim_t;     /* tick inizio animazione */
};
void gui_init(void);
void gui_run(void);   /* loop principale 60fps, non ritorna */
void gui_run_once_test(void);
gui_window_t* gui_open(int kind, const char* title);
void gui_close(gui_window_t* w);
void gui_set_theme(int t);
int gui_theme(void);
void sys_reboot(void);
void sys_poweroff(void);

/* apps: ogni app espone init/draw/input */
void app_terminal_draw(gui_window_t* w);
void app_terminal_key(gui_window_t* w, int key, char ch);
void app_terminal_click(gui_window_t* w, int x, int y, int btn);
void app_fileman_draw(gui_window_t* w);
void app_fileman_key(gui_window_t* w, int key, char ch);
void app_fileman_click(gui_window_t* w, int x, int y, int btn);
void app_calc_draw(gui_window_t* w);
void app_calc_key(gui_window_t* w, int key, char ch);
void app_calc_click(gui_window_t* w, int x, int y, int btn);
void app_browser_draw(gui_window_t* w);
void app_browser_key(gui_window_t* w, int key, char ch);
void app_browser_click(gui_window_t* w, int x, int y, int btn);
void app_editor_draw(gui_window_t* w);
void app_editor_key(gui_window_t* w, int key, char ch);
void app_editor_click(gui_window_t* w, int x, int y, int btn);
void app_sysmon_draw(gui_window_t* w);
void app_sysmon_key(gui_window_t* w, int key, char ch);
void app_sysmon_click(gui_window_t* w, int x, int y, int btn);

/* calc engine */
double calc_eval(const char* expr, int* ok);

/* browser engine */
void browser_navigate(void* priv, const char* url);
const char* browser_title(void* priv);
const char* browser_url(void* priv);

/* misc kernel */
void panic(const char* msg);
u32 mem_total_kb(void);
void mb2_main(u32 magic, u32 addr);   /* entry Multiboot2 (UEFI) */
void dbg_set_fb(u64 addr, u32 pitch, u32 w, u32 h);
void dbg_stage(u32 color);            /* schermo pieno: diagnostica stadi */
void led_blink(int n);                /* n blink CapsLock: diagnostica cieca */

#endif
