#include "plexos.h"

/* Driver framebuffer VBE/VESA via Multiboot (QEMU: VGA std + Bochs VBE).
   Supporto 720p: 1280x720x32 richiesti a GRUB.
   STABILE: double-buffering + vsync per eliminare flicker/tearing. */

static u32* fb_base = NULL;
static u32 fb_w = 0, fb_h = 0, fb_p = 0;
static u8  fb_depth = 0;
static int ready = 0;
static char fb_src[16] = "VGA-text";

/* Backbuffer RAM: fino a 1920x1080x32 (~8MB in .bss, non pesa sulla ISO).
   Copre 720p QEMU e pannelli reali 1080p ereditati via GOP. */
#define BB_MAX_W 1920
#define BB_MAX_H 1080
static u32 back_buffer[BB_MAX_W * BB_MAX_H];
static int use_bb = 0;

/* VGA text fallback se framebuffer non disponibile */
static u16* vga_text = (u16*)0xB8000;
static int vga_cx=0, vga_cy=0;

u32 TH_BG=0, TH_BAR=0, TH_ACCENT=0, TH_WINBG=0, TH_TITLE=0, TH_TEXT=0, TH_TASKBAR=0;

static void theme_default(void){
    TH_BG     = rgb(18,24,38);
    TH_BAR    = rgb(28,36,54);
    TH_ACCENT = rgb(0,180,255);
    TH_WINBG  = rgb(240,244,250);
    TH_TITLE  = rgb(30,42,64);
    TH_TEXT   = rgb(20,22,28);
    TH_TASKBAR= rgb(14,18,30);
}

u32 rgb(u8 r,u8 g,u8 b){ return ((u32)r<<16)|((u32)g<<8)|b; }

int fb_double_buffered(void){ return use_bb; }

void fb_vsync_wait(void){
    /* Attesa retrace verticale su VGA 0x3DA bit3.
       Su QEMU/Bochs VBE il porto potrebbe non cambiare: timeout rapido. */
    int t;
    t = 20000;
    while(t--){
        u8 s = inb(0x3DA);
        if(!(s & 0x08)) break;
    }
    t = 20000;
    while(t--){
        u8 s = inb(0x3DA);
        if(s & 0x08) break;
    }
}

void fb_present(void){
    if(!ready || !use_bb) return;
    /* Copia veloce backbuffer -> frontbuffer, riga per riga (pitch-aware).
       Salva EFLAGS: atomica senza alterare lo stato interrupt del chiamante. */
    u32 ef;
    __asm__ volatile("pushfl; popl %0; cli":"=r"(ef));
    for(u32 y = 0; y < fb_h; y++){
        u32* dst = (u32*)((u8*)fb_base + y * fb_p);
        u32* src = &back_buffer[y * fb_w];
        /* copia a 32-bit: il compilatore la trasforma in rep movsd */
        for(u32 x = 0; x < fb_w; x++) dst[x] = src[x];
    }
    __asm__ volatile("pushl %0; popfl"::"r"(ef));
}

int fb_init(multiboot_info_t* mbi){
    theme_default();
    kprintf("[FB] flags=%x fb: addr=%x pitch=%d %dx%dx%d type=%d\n",
        mbi->flags, mbi->fb_addr_lo, mbi->fb_pitch, mbi->fb_width, mbi->fb_height, mbi->fb_bpp, mbi->fb_type);
    if((mbi->flags>>12)&1){
        u32 addr = mbi->fb_addr_lo;
        u32 w = mbi->fb_width, h = mbi->fb_height;
        /* sanity su geometria GOP/VBE (HW reale): niente valori folli */
        if(addr && w>=640 && w<=4096 && h>=400 && h<=2160 &&
           mbi->fb_pitch>=w*4u && mbi->fb_pitch<=65536u){
            fb_base = (u32*)addr;
            fb_w = mbi->fb_width; fb_h = mbi->fb_height;
            fb_p = mbi->fb_pitch; fb_depth = mbi->fb_bpp;
            if(fb_depth==32 && fb_w>=640){
                ready=1;
                strcpy(fb_src, "GRUB");
                goto fb_ok;
            } else {
                kprintf("[FB] bpp=%d non 32, provo Bochs\n", fb_depth);
            }
        }
    }
    /* Piano 1.5: superficie REALE dal display Intel (Tiger Lake reale).
       Se sana e diversa da GRUB, vince lei (verita' del display engine).
       Blink 9 = adottata, 11 = plane spento. */
    {
        int has_grub_fb = ready;
        u32 grub_base = has_grub_fb ? (u32)fb_base : 0;
        if(intel_disp_probe()){
            u32 s = intel_disp_surf();
            if(s && (!has_grub_fb || (s&0xFFFFF000u)!=(grub_base&0xFFFFF000u))){
                if(!has_grub_fb){ fb_w=1920; fb_h=1080; fb_p=1920*4; fb_depth=32; }
                fb_base=(u32*)s;
                ready=1;
                strcpy(fb_src, "INTEL");
                kprintf("[FB] superficie Intel adottata: %x (GRUB diceva %x)\n", s, grub_base);
                led_blink(9);
                goto fb_ok;
            }
        }
    }
    /* Piano B: Bochs dispi diretto (QEMU/VGA std), 720p deterministico */
    {
        u32 lfb = bochs_set_mode(1280, 720, 32);
        if(lfb){
            fb_base=(u32*)lfb; fb_w=1280; fb_h=720; fb_p=1280*4; fb_depth=32;
            ready=1;
            strcpy(fb_src, "BOCHS");
            goto fb_ok;
        }
    }
    kprintf("[FB] framebuffer non disponibile, fallback VGA text\n");
    strcpy(fb_src, "VGA-text");
    /* diagnostica cieca: 7=fb oltre 4GB, 8=nessun fb (no VGA su UEFI),
       11=Intel vista ma plane spento */
    if(((mbi->flags>>12)&1) && mbi->fb_addr_hi) led_blink(7);
    else if(intel_disp_seen() && !intel_disp_on()) led_blink(11);
    else led_blink(8);
    ready=0;
    use_bb=0;
    return -1;

fb_ok:
    /* framebuffer veloce: write-combining via PAT (i3/HW reale) */
    if(cpu_has_pat_wc())
        paging_mark_wc((u32)fb_base, fb_p*fb_h);
    /* Attiva double-buffer se la risoluzione ci sta nel backbuffer */
    if(fb_w <= BB_MAX_W && fb_h <= BB_MAX_H){
        use_bb = 1;
        memset(back_buffer, 0, sizeof(back_buffer));
        kprintf("[FB] double-buffer ON (%dx%d)\n", fb_w, fb_h);
    } else {
        use_bb = 0;
        kprintf("[FB] risoluzione oltre 1920x1080: direct mode\n");
    }
    fb_clear(rgb(18,24,38));
    fb_present();
    kprintf("[FB] framebuffer OK via %s: %dx%dx%d @ %x\n", fb_src, fb_w, fb_h, fb_depth, (u32)fb_base);
    return 0;
}

const char* fb_source(void){ return fb_src; }

/* Schermata di fallback quando il display NON e' pronto:
   messaggio ROSSO su console VGA-text (BIOS) + serial.
   Spiega che il kernel e' vivo e che il video e' ancora in sviluppo.
   (Su UEFI cieco la console ottica non esiste: valgono i blink CapsLock 1-11.) */
static void vga_putstr(int row,int col,const char* s,int attr){
    u16* p=&vga_text[row*80+col];
    int i=0; while(s[i]&&col+i<80){ p[i]=(u16)((attr<<8)|(u8)s[i]); i++; }
}
void fb_dev_notice(void){
    serial_write("\n[FB/DISPLAY] NON INIZIALIZZATO\n[FB/DISPLAY] --> STILL IN DEVELOPMENT\n");
    for(int i=0;i<80*25;i++) vga_text[i]=0x0020;
    vga_putstr( 6,12,"PlexOS  -  display NOT initialized",0x04);
    vga_putstr( 9,10,"The kernel IS ALIVE (blind boot running).",0x0C);
    vga_putstr(10,10,"Video path on real hardware: develop only.",0x0C);
    vga_putstr(12,10,"More info: serial log + CAPS-LOCK blinks 1-11.",0x0C);
    vga_putstr(15,12,">>>  STILL IN DEVELOPMENT  <<<",0x0F);
    for(int c=0;c<80;c++) vga_text[17*80+c]=0x04C4;   /* riga rossa sotto */
    vga_putstr(19,10,"Press nothing - this box keeps running.",0x08);
    vga_putstr(20,10,"Try the QEMU release for the full desktop.",0x08);
}

u32 fb_width(void){ return ready?fb_w:80; }
u32 fb_height(void){ return ready?fb_h:25; }
u32 fb_pitch(void){ return fb_p; }
u8  fb_bpp(void){ return fb_depth; }
int fb_ready(void){ return ready; }
int fb_char_w(void){ return 8; }
int fb_char_h(void){ return 16; }

void fb_putpixel(int x,int y,u32 color){
    if(!ready) return;
    if(x<0||y<0||x>=(int)fb_w||y>=(int)fb_h) return;
    if(use_bb){
        back_buffer[y * fb_w + x] = color;
    } else {
        u32* p = (u32*)((u8*)fb_base + y*fb_p + x*4);
        *p = color;
    }
}
u32 fb_getpixel(int x,int y){
    if(!ready) return 0;
    if(x<0||y<0||x>=(int)fb_w||y>=(int)fb_h) return 0;
    if(use_bb) return back_buffer[y * fb_w + x];
    return *(u32*)((u8*)fb_base + y*fb_p + x*4);
}
void fb_clear(u32 color){
    if(!ready){
        for(int i=0;i<80*25;i++) vga_text[i]=0x0720;
        vga_cx=vga_cy=0; return;
    }
    if(use_bb){
        u32 n = fb_w * fb_h;
        for(u32 i=0;i<n;i++) back_buffer[i]=color;
    } else {
        for(u32 y=0;y<fb_h;y++){
            u32* row=(u32*)((u8*)fb_base+y*fb_p);
            for(u32 x=0;x<fb_w;x++) row[x]=color;
        }
    }
}
void fb_fill_rect(int x,int y,int w,int h,u32 color){
    if(!ready) return;
    if(w<=0||h<=0) return;
    if(x<0){w+=x;x=0;} if(y<0){h+=y;y=0;}
    if(x+w>(int)fb_w) w=fb_w-x; if(y+h>(int)fb_h) h=fb_h-y;
    if(w<=0||h<=0) return;
    if(use_bb){
        for(int j=0;j<h;j++){
            u32* row=&back_buffer[(y+j)*fb_w+x];
            for(int i=0;i<w;i++) row[i]=color;
        }
    } else {
        for(int j=0;j<h;j++){
            u32* row=(u32*)((u8*)fb_base+(y+j)*fb_p+x*4);
            for(int i=0;i<w;i++) row[i]=color;
        }
    }
}
void fb_draw_rect(int x,int y,int w,int h,u32 color){
    fb_fill_rect(x,y,w,1,color); fb_fill_rect(x,y+h-1,w,1,color);
    fb_fill_rect(x,y,1,h,color); fb_fill_rect(x+w-1,y,1,h,color);
}
void fb_draw_line(int x0,int y0,int x1,int y1,u32 color){
    int dx=x1-x0; if(dx<0)dx=-dx;
    int sx=x0<x1?1:-1;
    int dy=y1-y0; if(dy<0)dy=-dy; dy=-dy;
    int sy=y0<y1?1:-1;
    int err=dx+dy;
    while(1){
        fb_putpixel(x0,y0,color);
        if(x0==x1&&y0==y1)break;
        int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}
void fb_draw_circle(int cx,int cy,int r,u32 color){
    int x=r,y=0,e=0;
    while(x>=y){
        fb_putpixel(cx+x,cy+y,color); fb_putpixel(cx+y,cy+x,color);
        fb_putpixel(cx-y,cy+x,color); fb_putpixel(cx-x,cy+y,color);
        fb_putpixel(cx-x,cy-y,color); fb_putpixel(cx-y,cy-x,color);
        fb_putpixel(cx+y,cy-x,color); fb_putpixel(cx+x,cy-y,color);
        y++; if(e<=0) e+=2*y+1; else {x--; e-=2*x+1;}
    }
}
void fb_fill_circle(int cx,int cy,int r,u32 color){
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++)
        if(x*x+y*y<=r*r) fb_putpixel(cx+x,cy+y,color);
}

/* carattere 8x16: raddoppia verticalmente il font 8x8 */
void fb_draw_char(int x,int y,char c,u32 fg,u32 bg,int transparent){
    if(!ready){
        u16 v=(0x07<<8)|((u8)c);
        if(vga_cx>=0&&vga_cx<80&&vga_cy>=0&&vga_cy<25)
            vga_text[vga_cy*80+vga_cx]=v;
        return;
    }
    unsigned char uc=(unsigned char)c;
    if(uc<32||uc>126){
        if(!transparent) fb_fill_rect(x,y,8,16,bg);
        return;
    }
    for(int row=0;row<8;row++){
        u8 bits=font_row(c,row);
        for(int sy=0;sy<2;sy++){
            int yy=y+row*2+sy;
            if(yy<0||yy>=(int)fb_h) continue;
            for(int col=0;col<8;col++){
                int on=(bits>>(7-col))&1;
                int xx=x+col;
                if(xx<0||xx>=(int)fb_w) continue;
                if(on){
                    if(use_bb) back_buffer[yy*fb_w+xx]=fg;
                    else *(u32*)((u8*)fb_base+yy*fb_p+xx*4)=fg;
                } else if(!transparent){
                    if(use_bb) back_buffer[yy*fb_w+xx]=bg;
                    else *(u32*)((u8*)fb_base+yy*fb_p+xx*4)=bg;
                }
            }
        }
    }
}
void fb_draw_string_bg(int x,int y,const char* s,u32 fg,u32 bg){
    for(int i=0;s[i];i++) fb_draw_char(x+i*8,y,s[i],fg,bg,0);
}
void fb_draw_string(int x,int y,const char* s,u32 color){
    for(int i=0;s[i];i++) fb_draw_char(x+i*8,y,s[i],color,0,1);
}
