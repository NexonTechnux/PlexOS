#include "plexos.h"

/* App 4: PlexBrowse - browser offline PlexNet */

typedef struct {
    int inited;
    char url[128];
    char addr[128];
    int editing;
    char hist[8][128];
    int hpos, hcount;
    int scroll;
} br_t;

typedef struct { const char* url; const char* title; const char* body; } page_t;

static const page_t pages[] = {
    {"plex://home","PlexOS Home",
     "Benvenuto su PlexNet (offline)!\n"
     "==============================\n"
     "Questo e' il browser di PlexOS.\n"
     "Pagine disponibili:\n"
     " - plex://home (questa pagina)\n"
     " - plex://apps (le 6 app)\n"
     " - plex://guida (comandi terminale)\n"
     " - plex://qemu (driver QEMU)\n"
     " - plex://about (info sistema)\n"
     "Usa la barra indirizzi + Invio.\n"},
    {"plex://apps","Le 6 App",
     "Le 6 applicazioni PlexOS (NexonTech):\n"
     "1. PlexTerm - terminale + shell\n"
     "2. PlexFiles - file manager\n"
     "3. PlexCalc - calcolatrice\n"
     "4. PlexBrowse - questo browser\n"
     "5. PlexEdit - editor di testo\n"
     "6. Impostazioni - sistema/rete/temi\n"
     "Aprile dal menu Start.\n"},
    {"plex://guida","Guida comandi",
     "Comandi shell PlexOS:\n"
     "help ls cd pwd mkdir touch cat\n"
     "echo write rm cp mv clear\n"
     "sysinfo meminfo pci ps uptime\n"
     "date calc theme apps reboot\n"
     "Esempio: calc 2*(3+4)^2\n"},
    {"plex://qemu","Driver QEMU",
     "Driver QEMU in PlexOS 1.0:\n"
     "- Framebuffer VBE 1280x720x32\n"
     "- Tastiera i8042 + Mouse PS/2\n"
     "- Timer PIT 100Hz\n"
     "- Seriale COM1 / 0xE9 debug\n"
     "- PCI enumeration bus 0-3\n"
     "- ATA PIO primary master\n"
     "- RTC CMOS data/ora\n"
     "Risoluzione: 720p HD.\n"},
    {"plex://about","About PlexOS",
     "PlexOS 1.0.0 - 720p@60fps\n"
     "Kernel 32-bit Multiboot\n"
     "GUI 720p + Window Manager\n"
     "CPU Intel Core i3-1115G4 ready\n"
     "GPU Intel UHD Graphics G4 ready\n"
     "Rete Intel e1000 generica\n"
     "FS in-RAM + ATA driver\n"
     "di NexonTech\n"
     "QEMU + hardware reale.\n"},
};
#define NPAGES (sizeof(pages)/sizeof(pages[0]))

static const page_t* find_page(const char* url){
    for(unsigned i=0;i<NPAGES;i++) if(strcmp(pages[i].url,url)==0) return &pages[i];
    return NULL;
}

void browser_navigate(void* p, const char* url){
    br_t* b=(br_t*)p;
    strncpy(b->url,url,127); strncpy(b->addr,url,127);
    b->scroll=0; b->editing=0;
    if(b->hcount==0||strcmp(b->hist[b->hpos],url)!=0){
        if(b->hcount<8){ b->hpos=b->hcount; strcpy(b->hist[b->hcount++],url); }
        else { for(int i=0;i<7;i++) strcpy(b->hist[i],b->hist[i+1]); strcpy(b->hist[7],url); b->hpos=7; }
    }
}
const char* browser_title(void* p){
    const page_t* pg=find_page(((br_t*)p)->url);
    return pg?pg->title:"404";
}
const char* browser_url(void* p){ return ((br_t*)p)->url; }

static void b_init(br_t* b){
    b->inited=1; b->editing=0; b->scroll=0; b->hcount=0; b->hpos=0;
    browser_navigate(b,"plex://home");
}

void app_browser_draw(gui_window_t* w){
    br_t* b=(br_t*)w->priv;
    if(!b->inited) b_init(b);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(255,255,255));
    /* toolbar */
    fb_fill_rect(cx,cy,cw,30,rgb(230,235,245));
    /* back fwd */
    fb_fill_rect(cx+6,cy+4,30,22,TH_TITLE); fb_draw_string(cx+12,cy+7,"<",rgb(255,255,255));
    fb_fill_rect(cx+40,cy+4,30,22,TH_TITLE); fb_draw_string(cx+46,cy+7,">",rgb(255,255,255));
    /* address */
    fb_fill_rect(cx+76,cy+4,cw-160,22,rgb(255,255,255));
    fb_draw_rect(cx+76,cy+4,cw-160,22,rgb(100,100,100));
    char ab[64]; strncpy(ab,b->addr,40); ab[40]=0;
    fb_draw_string(cx+82,cy+7,ab,TH_TEXT);
    if(b->editing&&w->focused&&((timer_ticks()/30)%2==0)){
        fb_fill_rect(cx+82+strlen(ab)*8,cy+7,8,14,TH_ACCENT);
    }
    fb_fill_rect(cx+cw-78,cy+4,70,22,TH_ACCENT);
    fb_draw_string(cx+cw-62,cy+7,"Vai",rgb(0,0,0));
    /* quick links */
    int qy=cy+34;
    const char* q[5]={"home","apps","guida","qemu","about"};
    int qx=cx+8;
    for(int i=0;i<5;i++){
        fb_fill_rect(qx,qy,80,20,rgb(220,235,255));
        fb_draw_rect(qx,qy,80,20,TH_ACCENT);
        fb_draw_string(qx+8,qy+3,q[i],rgb(20,40,80));
        qx+=86;
    }
    /* contenuto */
    const page_t* pg=find_page(b->url);
    int ty=qy+28;
    if(!pg){
        fb_draw_string(cx+8,ty,"404 - pagina non trovata",rgb(200,0,0));
        fb_draw_string(cx+8,ty+20,"Prova plex://home",rgb(100,100,100));
        return;
    }
    char ttl[128]; ksnprintf(ttl,sizeof(ttl),"%s - %s", pg->title, pg->url);
    fb_draw_string(cx+8,ty,ttl,TH_TITLE);
    ty+=22;
    fb_draw_line(cx+8,ty,cx+cw-8,ty,rgb(200,200,200)); ty+=8;
    /* body word-wrap semplice per linee */
    char body[1024]; strncpy(body,pg->body,1023); body[1023]=0;
    int maxc=(cw-24)/8;
    int line=0;
    char* p=body;
    char lbuf[160];
    while(*p && ty+16<cy+ch-8){
        /* prendi fino a \n */
        int i=0;
        while(p[i]&&p[i]!='\n'&&i<maxc){ lbuf[i]=p[i]; i++; }
        lbuf[i]=0;
        fb_draw_string(cx+8,ty+b->scroll,lbuf,TH_TEXT);
        ty+=18;
        if(p[i]=='\n') p+=i+1; else p+=i;
        line++;
        if(line>40) break;
    }
}

void app_browser_key(gui_window_t* w, int key, char ch){
    br_t* b=(br_t*)w->priv;
    if(!b->inited) b_init(b);
    UNUSED(key);
    if(ch=='\n'){
        if(b->addr[0]) browser_navigate(b,b->addr);
        return;
    }
    if(ch=='\b'){ int l=strlen(b->addr); if(l)b->addr[l-1]=0; b->editing=1; return; }
    if(ch>=32&&ch<127){ int l=strlen(b->addr); if(l<120){b->addr[l]=ch;b->addr[l+1]=0;} b->editing=1; }
}
void app_browser_click(gui_window_t* w, int x, int y, int btn){
    br_t* b=(br_t*)w->priv;
    if(!b->inited) b_init(b);
    if(!(btn&1)) return;
    int cw=w->w-12;
    /* back/fwd */
    if(y>=4&&y<26){
        if(x>=6&&x<36){ if(b->hpos>0){b->hpos--; strcpy(b->url,b->hist[b->hpos]); strcpy(b->addr,b->url);} return; }
        if(x>=40&&x<70){ if(b->hpos<b->hcount-1){b->hpos++; strcpy(b->url,b->hist[b->hpos]); strcpy(b->addr,b->url);} return; }
        if(x>=cw-78&&x<cw-8){ if(b->addr[0]) browser_navigate(b,b->addr); return; }
        if(x>=76&&x<cw-84){ b->editing=1; return; }
    }
    /* quick links */
    if(y>=34&&y<54){
        const char* urls[5]={"plex://home","plex://apps","plex://guida","plex://qemu","plex://about"};
        int qx=8;
        for(int i=0;i<5;i++){
            if(x>=qx&&x<qx+80){ browser_navigate(b,urls[i]); return; }
            qx+=86;
        }
    }
}
