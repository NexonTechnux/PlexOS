#include "plexos.h"

#define MAXW 12

static gui_window_t wins[MAXW];
static int next_id=1;
static int theme_idx=0;
static int start_open=0;
static int zorder[MAXW];
static int zcount=0;

static const char* app_names[6]={"Terminale","File Manager","Calcolatrice","Browser","Editor","Impostazioni"};
static u32 start_anim_t=0; /* inizio animazione menu start */

void gui_set_theme(int t){
    theme_idx=t;
    if(t==1){ /* chiaro */
        TH_BG=rgb(200,212,230); TH_BAR=rgb(60,90,150); TH_ACCENT=rgb(0,120,220);
        TH_WINBG=rgb(250,250,252); TH_TITLE=rgb(40,60,120); TH_TEXT=rgb(10,10,10); TH_TASKBAR=rgb(40,50,70);
    } else if(t==2){ /* blu */
        TH_BG=rgb(8,30,70); TH_BAR=rgb(10,50,120); TH_ACCENT=rgb(0,220,255);
        TH_WINBG=rgb(235,242,255); TH_TITLE=rgb(10,40,110); TH_TEXT=rgb(10,15,30); TH_TASKBAR=rgb(5,15,40);
    } else { /* scuro default */
        theme_idx=0;
        TH_BG=rgb(18,24,38); TH_BAR=rgb(28,36,54); TH_ACCENT=rgb(0,180,255);
        TH_WINBG=rgb(240,244,250); TH_TITLE=rgb(30,42,64); TH_TEXT=rgb(20,22,28); TH_TASKBAR=rgb(14,18,30);
    }
}
int gui_theme(void){ return theme_idx; }

static void z_bring(int idx){
    int pos=-1;
    for(int i=0;i<zcount;i++) if(zorder[i]==idx){pos=i;break;}
    if(pos>=0){ for(int i=pos;i<zcount-1;i++) zorder[i]=zorder[i+1]; zcount--; }
    zorder[zcount++]=idx;
    for(int i=0;i<MAXW;i++) wins[i].focused=0;
    wins[idx].focused=1;
}

gui_window_t* gui_open(int kind, const char* title){
    for(int i=0;i<MAXW;i++) if(!wins[i].used){
        wins[i].used=1; wins[i].id=next_id++;
        wins[i].kind=kind;
        strncpy(wins[i].title,title?title:app_names[kind],63);
        int W=fb_width(), H=fb_height();
        int w=640,h=420;
        if(kind==0){w=680;h=440;}
        if(kind==1){w=700;h=440;}
        if(kind==3){w=720;h=460;}
        if(kind==4){w=680;h=440;}
        if(w>W-40)w=W-40; if(h>H-80)h=H-80;
        wins[i].w=w; wins[i].h=h;
        wins[i].x=(W-w)/2 + (i*24)%120 - 60;
        wins[i].y=(H-h)/2 + (i*18)%80 - 20;
        if(wins[i].x<0)wins[i].x=0; if(wins[i].y<0)wins[i].y=0;
        wins[i].focused=0; wins[i].minimized=0; wins[i].closed=0;
        wins[i].dragging=0; wins[i].scroll=0;
        wins[i].anim=1; wins[i].anim_t=timer_ticks(); /* animazione apertura */
        wins[i].priv=kcalloc(1,8192);
        if(!wins[i].priv){ wins[i].used=0; return NULL; }
        z_bring(i);
        kprintf("[GUI] aperta finestra %s kind=%d\n", wins[i].title, kind);
        return &wins[i];
    }
    return NULL;
}
void gui_close(gui_window_t* w){
    if(!w) return;
    if(w->priv) kfree(w->priv);
    w->priv=NULL; w->used=0; w->closed=1;
    int pos=-1;
    for(int i=0;i<zcount;i++) if(&wins[zorder[i]]==w){pos=i;break;}
    if(pos>=0){ for(int i=pos;i<zcount-1;i++) zorder[i]=zorder[i+1]; zcount--; }
    if(zcount>0) wins[zorder[zcount-1]].focused=1;
}

static gui_window_t* focused_win(void){
    if(zcount==0) return NULL;
    gui_window_t* w=&wins[zorder[zcount-1]];
    if(w->used&&!w->minimized) return w;
    for(int i=zcount-1;i>=0;i--){
        gui_window_t* c=&wins[zorder[i]];
        if(c->used&&!c->minimized) return c;
    }
    return NULL;
}

static void draw_wallpaper(void){
    int W=fb_width(), H=fb_height();
    /* Gradiente veloce: 1 fill_rect per riga (niente per-pixel) */
    for(int y=0;y<H;y++){
        u8 r,g,b;
        if(theme_idx==1){ r=200-(y*40/H); g=212-(y*30/H); b=230-(y*20/H); }
        else if(theme_idx==2){ r=8; g=30+(y*40/H); b=70+(y*60/H); }
        else { r=18+(y*20/H); g=24+(y*30/H); b=38+(y*50/H); }
        fb_fill_rect(0,y,W,1,rgb(r,g,b));
    }
    /* logo centrale semi-trasparente: scritta PlexOS */
    const char* logo="PlexOS";
    int lx=W/2-3*8-40, ly=H/2-60;
    /* pannello benvenuto dietro le finestre */
    fb_fill_rect(W/2-220, 90, 440, 90, rgb(0,0,0));
    /* bordo accent */
    /* simuliamo alpha disegnando scuro: usiamo rettangolo con colore taskbar */
    fb_fill_rect(W/2-220, 90, 440, 90, TH_TASKBAR);
    fb_draw_rect(W/2-220, 90, 440, 90, TH_ACCENT);
    fb_draw_string(W/2-200, 100, "Benvenuto in PlexOS 1.0 - 720p HD", rgb(255,255,255));
    fb_draw_string(W/2-200, 120, "Start in basso a sinistra - click sulle icone", rgb(160,200,255));
    fb_draw_string(W/2-200, 140, "Terminale: scrivi 'help' per i comandi", rgb(160,200,255));
    UNUSED(logo); UNUSED(lx); UNUSED(ly);
}

static void draw_window(gui_window_t* w){
    int x=w->x, y=w->y, ww=w->w, hh=w->h;
    /* ombra */
    fb_fill_rect(x+4,y+4,ww,hh,rgb(0,0,0));
    /* corpo */
    fb_fill_rect(x,y,ww,hh,TH_WINBG);
    /* barra titolo */
    u32 tc = w->focused?TH_TITLE:rgb(90,100,120);
    fb_fill_rect(x,y,ww,26,tc);
    if(w->focused) fb_fill_rect(x,y,ww,3,TH_ACCENT);
    fb_draw_string(x+10,y+6,w->title,rgb(255,255,255));
    /* pulsanti */
    fb_fill_rect(x+ww-52,y+4,20,18,rgb(255,190,0));
    fb_draw_string(x+ww-46,y+5,"_",rgb(0,0,0));
    fb_fill_rect(x+ww-28,y+4,20,18,rgb(255,80,80));
    fb_draw_string(x+ww-22,y+5,"X",rgb(255,255,255));
    fb_draw_rect(x,y,ww,hh,rgb(0,0,0));
    /* contenuto */
    int cx=x+6, cy=y+32, cw=ww-12, ch=hh-38;
    /* clip semplice: disegna sfondo contenuto */
    fb_fill_rect(cx,cy,cw,ch,rgb(255,255,255));
    /* chiama draw app con traslazione: le app disegnano in coordinate assolute usando w->x/y */
    switch(w->kind){
        case 0: app_terminal_draw(w); break;
        case 1: app_fileman_draw(w); break;
        case 2: app_calc_draw(w); break;
        case 3: app_browser_draw(w); break;
        case 4: app_editor_draw(w); break;
        case 5: app_sysmon_draw(w); break;
    }
    UNUSED(cx); UNUSED(cy); UNUSED(cw); UNUSED(ch);
}

static void draw_taskbar(void){
    int W=fb_width(), H=fb_height();
    int th=38;
    fb_fill_rect(0,H-th,W,th,TH_TASKBAR);
    fb_fill_rect(0,H-th,W,2,TH_ACCENT);
    /* start */
    fb_fill_rect(6,H-th+6,110,th-12,TH_ACCENT);
    fb_draw_string(20,H-th+13,"Plex Start",rgb(0,0,0));
    /* finestre */
    int bx=126;
    for(int i=0;i<zcount&&bx<W-180;i++){
        gui_window_t* w=&wins[zorder[i]];
        if(!w->used) continue;
        u32 c=w->focused?rgb(60,80,120):rgb(35,45,65);
        fb_fill_rect(bx,H-th+6,120,th-12,c);
        char t[18]; strncpy(t,w->title,17); t[17]=0;
        /* tronca */
        if(strlen(t)>14){t[14]=0;strcat(t,"..");}
        fb_draw_string(bx+6,H-th+13,t,rgb(255,255,255));
        bx+=126;
    }
    /* orologio */
    rtc_time_t rt; rtc_read(&rt);
    char clk[16]; ksnprintf(clk,sizeof(clk),"%02d:%02d:%02d",rt.hour,rt.min,rt.sec);
    fb_draw_string(W-90,H-th+13,clk,rgb(255,255,255));
    /* start menu con animazione scorrimento + tasti rosso power-off / reboot */
    if(start_open){
        int mw=230, mh=314;
        int mx=6, my=H-th-mh-6;
        /* slide-up: offset da 50px a 0 in 120ms */
        {
            u32 dt=timer_ticks()-start_anim_t;
            if(dt<12){ int p=dt; my+=(12-p)*4; }
        }
        fb_fill_rect(mx,my,mw,mh,rgb(245,247,252));
        fb_draw_rect(mx,my,mw,mh,rgb(20,20,20));
        fb_fill_rect(mx,my,mw,30,TH_TITLE);
        fb_draw_string(mx+10,my+8,"PlexOS - Applicazioni",rgb(255,255,255));
        for(int i=0;i<6;i++){
            int iy=my+40+i*34;
            fb_fill_rect(mx+8,iy,mw-16,28,TH_WINBG);
            fb_draw_rect(mx+8,iy,mw-16,28,TH_ACCENT);
            char b[64]; ksnprintf(b,sizeof(b),"%d. %s", i+1, app_names[i]);
            fb_draw_string(mx+16,iy+7,b,TH_TEXT);
        }
        /* tasti alimentazione: rosso spegni + arancione riavvia */
        {
            int py=my+40+6*34+6;
            fb_fill_rect(mx+8,py,104,30,rgb(210,30,30));
            fb_draw_rect(mx+8,py,104,30,rgb(120,0,0));
            fb_draw_string(mx+14,py+8,"SPEGNI",rgb(255,255,255));
            fb_fill_rect(mx+118,py,104,30,rgb(230,140,20));
            fb_draw_rect(mx+118,py,104,30,rgb(120,60,0));
            fb_draw_string(mx+124,py+8,"RIAVVIA",rgb(0,0,0));
        }
        fb_draw_string(mx+10,my+mh-16,"Click app / rosso=off",rgb(120,120,120));
    }
}

static void draw_cursor(void){
    int x=mouse_x(), y=mouse_y();
    /* freccia classica 12x18 */
    static const char* arrow[18]={
        "X...........",
        "XX..........",
        "XXX.........",
        "XXXX........",
        "XXXXX.......",
        "XXXXXX......",
        "XXXXXXX.....",
        "XXXXXXXX....",
        "XXXXXXXXX...",
        "XXXXXXXXXX..",
        "XXXXXXX.....",
        "XX.XXXX.....",
        "X..XXXX.....",
        "...XXXX.....",
        "...XXXX.....",
        "....XX......",
        "............",
        "............",
    };
    for(int r=0;r<16;r++) for(int c=0;c<11;c++){
        if(arrow[r][c]=='X'){
            fb_putpixel(x+c,y+r,rgb(255,255,255));
        }
    }
    /* contorno */
    for(int r=0;r<16;r++){
        int last=-1;
        for(int c=0;c<11;c++) if(arrow[r][c]=='X') last=c;
        if(last>=0){
            fb_putpixel(x-1,y+r,rgb(0,0,0));
            fb_putpixel(x+last+1,y+r,rgb(0,0,0));
        }
    }
    for(int c=0;c<11;c++){
        if(arrow[0][c]=='X'){ fb_putpixel(x+c,y-1,rgb(0,0,0)); break; }
    }
}

static int point_in(int px,int py,int x,int y,int w,int h){
    return px>=x&&px<x+w&&py>=y&&py<y+h;
}

void gui_init(void){
    memset(wins,0,sizeof(wins));
    zcount=0;
    gui_set_theme(0);
    gui_open(0,"PlexTerm - Terminale");
    gui_open(1,"PlexFiles - File Manager");
    /* minimizza file manager all'avvio? no, porta terminale in primo piano */
    z_bring(0);
    kprintf("[GUI] desktop pronto %dx%d\n", fb_width(), fb_height());
}

void gui_run(void){
    kprintf("[GUI] loop avviato (720p@60fps dbl-buf vsync anim)\n");
    int prev_btn=0;
    int last_mx=mouse_x(), last_my=mouse_y();
    u32 last_present=timer_ticks();
    u32 last_blink=timer_ticks();
    int last_clk_sec=-1;
    int needs_redraw=1; /* primo frame */
    /* presenta subito il desktop */
    draw_wallpaper();
    for(int i=0;i<zcount;i++){
        gui_window_t* w=&wins[zorder[i]];
        if(!w->used||w->minimized) continue;
        draw_window(w);
    }
    draw_taskbar();
    draw_cursor();
    fb_vsync_wait();
    fb_present();
    last_present=timer_ticks();
    needs_redraw=0;

    while(1){
        int activity=0; /* qualcosa ha cambiato lo schermo? */
        /* --- input tastiera -> finestra focus --- */
        gui_window_t* fw=focused_win();
        while(kbd_has_char()||kbd_has_key()){
            char ch=0; int key=0;
            if(kbd_has_char()) ch=kbd_getchar();
            else if(kbd_has_key()) key=kbd_getkey();
            else break;
            if(key==KEY_MENU){ /* F10: Plex Start ovunque */
                start_open=!start_open;
                start_anim_t=timer_ticks();
                activity=1;
                continue;
            }
            if(start_open&&ch>='1'&&ch<='6'){ /* scorciatoia menu */
                int k=ch-'1';
                gui_open(k,app_names[k]);
                start_open=0;
                activity=1;
                continue;
            }
            if(!fw) continue;
            switch(fw->kind){
                case 0: app_terminal_key(fw,key,ch); break;
                case 1: app_fileman_key(fw,key,ch); break;
                case 2: app_calc_key(fw,key,ch); break;
                case 3: app_browser_key(fw,key,ch); break;
                case 4: app_editor_key(fw,key,ch); break;
                case 5: app_sysmon_key(fw,key,ch); break;
            }
            activity=1;
        }
        /* --- mouse --- */
        int mx=mouse_x(), my=mouse_y(), btn=mouse_buttons();
        if(mx!=last_mx||my!=last_my||btn!=prev_btn) activity=1;
        last_mx=mx; last_my=my;
        int clicked = (btn&1)&&!(prev_btn&1);
        int released = !(btn&1)&&(prev_btn&1);
        UNUSED(released);
        /* trascinamento finestre */
        if(btn&1){
            for(int i=zcount-1;i>=0;i--){
                gui_window_t* w=&wins[zorder[i]];
                if(!w->used||w->minimized) continue;
                if(w->dragging){
                    int nx=mx-w->drag_ox, ny=my-w->drag_oy;
                    if(nx<0)nx=0; if(ny<0)ny=0;
                    if(nx+w->w>(int)fb_width())nx=fb_width()-w->w;
                    if(ny<0)ny=0;
                    if(nx!=w->x||ny!=w->y){ w->x=nx; w->y=ny; activity=1; }
                    else activity=1; /* durante il drag ridisegna comunque */
                    break;
                }
            }
        } else {
            for(int i=0;i<MAXW;i++) wins[i].dragging=0;
        }

        if(clicked){
            activity=1;
            int W=fb_width(), H=fb_height();
            int th=38;
            /* click su start */
            if(point_in(mx,my,6,H-th+6,110,th-12)){
                start_open=!start_open;
                start_anim_t=timer_ticks();
            }
            /* click su voce start menu (mh=314) */
            else if(start_open && point_in(mx,my,6,H-th-314-6,230,314)){
                int my0=H-th-314-6;
                int done=0;
                for(int i=0;i<6&&!done;i++){
                    int iy=my0+40+i*34;
                    if(point_in(mx,my,6+8,iy,230-16,28)){
                        gui_open(i,app_names[i]);
                        start_open=0;
                        done=1;
                        break;
                    }
                }
                if(!done){
                    /* tasti rosso power-off / reboot */
                    int py=my0+40+6*34+6;
                    if(point_in(mx,my,6+8,py,104,30)){ sys_poweroff(); }
                    else if(point_in(mx,my,6+118,py,104,30)){ sys_reboot(); }
                }
            }
            /* click su taskbar window buttons */
            else if(my>=H-th){
                int bx=126;
                for(int i=0;i<zcount;i++){
                    gui_window_t* w=&wins[zorder[i]];
                    if(!w->used) continue;
                    if(point_in(mx,my,bx,H-th+6,120,th-12)){
                        if(w->minimized){ w->minimized=0; w->anim=3; w->anim_t=timer_ticks(); }
                        z_bring(zorder[i]);
                        /* ricalcola: zorder cambiato, esci */
                        break;
                    }
                    bx+=126;
                }
                if(point_in(mx,my,6,H-th+6,110,th-12)){} /* gia' gestito */
                else start_open=0;
            }
            else {
                if(start_open && !point_in(mx,my,6,H-th-314-6,230,314)) start_open=0;
                /* cerca finestra dall'alto */
                int hit=-1;
                for(int i=zcount-1;i>=0;i--){
                    gui_window_t* w=&wins[zorder[i]];
                    if(!w->used||w->minimized) continue;
                    if(point_in(mx,my,w->x,w->y,w->w,w->h)){ hit=zorder[i]; break; }
                }
                if(hit>=0){
                    gui_window_t* w=&wins[hit];
                    z_bring(hit);
                    /* pulsanti chiusura/min */
                    if(point_in(mx,my,w->x+w->w-28,w->y+4,20,18)){ gui_close(w); }
                    else if(point_in(mx,my,w->x+w->w-52,w->y+4,20,18)){ w->anim=2; w->anim_t=timer_ticks(); }
                    else if(point_in(mx,my,w->x,w->y,w->w,26)){
                        w->dragging=1; w->drag_ox=mx-w->x; w->drag_oy=my-w->y;
                    } else {
                        /* click contenuto */
                        int lx=mx-(w->x+6), ly=my-(w->y+32);
                        switch(w->kind){
                            case 0: app_terminal_click(w,lx,ly,btn); break;
                            case 1: app_fileman_click(w,lx,ly,btn); break;
                            case 2: app_calc_click(w,lx,ly,btn); break;
                            case 3: app_browser_click(w,lx,ly,btn); break;
                            case 4: app_editor_click(w,lx,ly,btn); break;
                            case 5: app_sysmon_click(w,lx,ly,btn); break;
                        }
                    }
                }
            }
        }
        prev_btn=btn;
        if(activity) needs_redraw=1;

        /* completa minimizzazioni animate (120ms) */
        for(int i=0;i<MAXW;i++){
            if(wins[i].used&&wins[i].anim==2&&(timer_ticks()-wins[i].anim_t)>=12){
                wins[i].anim=0; wins[i].minimized=1; needs_redraw=1;
            }
        }
        /* animazioni attive? -> forza 60fps */
        int anim_active=0;
        for(int i=0;i<MAXW;i++) if(wins[i].used&&wins[i].anim!=0) anim_active=1;
        if(start_open&&(timer_ticks()-start_anim_t)<12) anim_active=1;
        for(int i=0;i<MAXW;i++){
            if(wins[i].used&&wins[i].dragging) anim_active=1;
        }

        /* --- draw 720p@60fps: vsync + double-buffer, dirty quando fermo --- */
        if(timer_ticks()-last_blink>=50){ /* 0.5s blink */
            last_blink=timer_ticks();
            needs_redraw=1;
        }
        {
            rtc_time_t __rt; rtc_read(&__rt);
            if(__rt.sec!=last_clk_sec){ last_clk_sec=__rt.sec; needs_redraw=1; }
            net_poll(); /* drena pacchetti Intel e1000 */
            if(net_rx_count()+net_tx_count()>0){ /* traffico? ridisegna 1Hz via clock */ }
        }
        if(anim_active) needs_redraw=1;

        /* 60fps: 1 tick=10ms durante animazioni (vsync blocca a 60 su HW),
           2 tick=20ms (~50fps) da fermo per stabilita' QEMU */
        {
            u32 elapsed=timer_ticks()-last_present;
            u32 need=anim_active?1:2;
            if(needs_redraw && elapsed>=need){
                draw_wallpaper();
                for(int i=0;i<zcount;i++){
                    gui_window_t* w=&wins[zorder[i]];
                    if(!w->used) continue;
                    if(w->minimized&&w->anim!=2) continue;
                    /* slide animata: offset verticale temporaneo */
                    int oy=0;
                    if(w->anim!=0){
                        u32 dt=timer_ticks()-w->anim_t;
                        if(dt>12)dt=12;
                        if(w->anim==1||w->anim==3) oy=(12-(int)dt)*5;      /* sale da +60px */
                        else if(w->anim==2) oy=(int)dt*12;                 /* scende a +144px */
                        if(w->anim==1||w->anim==3){
                            if(dt>=12){ w->anim=0; oy=0; }
                        }
                    }
                    if(oy!=0){ w->y+=oy; draw_window(w); w->y-=oy; }
                    else draw_window(w);
                }
                draw_taskbar();
                draw_cursor();
                fb_vsync_wait();
                fb_present();
                last_present=timer_ticks();
                needs_redraw=0;
            } else {
                sleep_ms(2); /* reattivo a 60fps */
            }
        }
    }
}
