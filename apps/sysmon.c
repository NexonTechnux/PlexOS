#include "plexos.h"

/* App 6: Impostazioni - Sistema / Internet / Temi (NexonTech).
   Struttura estendibile: nuove voci si aggiungono come tab. */

typedef struct { int inited; int tab; char msg[96]; } sys_t;
static void s_init(sys_t* s){ s->inited=1; s->tab=0; s->msg[0]=0; }

void app_sysmon_draw(gui_window_t* w){
    sys_t* s=(sys_t*)w->priv;
    if(!s->inited) s_init(s);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(245,247,252));
    const char* tabs[3]={"[Sistema]","[Internet]","[Tema]"};
    int tx=cx+8, ty=cy+6;
    for(int i=0;i<3;i++){
        u32 bg=(s->tab==i)?TH_ACCENT:rgb(200,210,230);
        fb_fill_rect(tx,ty,100,24,bg);
        fb_draw_string(tx+6,ty+5,tabs[i],rgb(0,0,0));
        tx+=106;
    }
    int by=ty+36;
    if(s->tab==0){
        char b[256];
        rtc_time_t t; rtc_read(&t); char ds[32]; rtc_format(ds,&t);
        ksnprintf(b,sizeof(b),"PlexOS 1.0.0 - 720p@60fps (%dx%dx%d) - NexonTech", fb_width(),fb_height(),fb_bpp());
        fb_draw_string(cx+12,by,b,TH_TITLE); by+=20;
        ksnprintf(b,sizeof(b),"CPU: %s", cpu_short());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        ksnprintf(b,sizeof(b),"  %d core %dMHz base%d | %s", cpu_cores(), cpu_mhz(), cpu_base_mhz(), cpu_brand());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        { char cc[64]; cpu_cache_str(cc,sizeof(cc));
          ksnprintf(b,sizeof(b),"Cache %s | %s", cc, cpu_feats());
          fb_draw_string(cx+12,by,b,TH_TEXT); by+=18; }
        ksnprintf(b,sizeof(b),"GPU: %s", gpu_name());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        ksnprintf(b,sizeof(b),"RAM: %d KB tot / %d KB liberi  Heap: %d B",
            pmm_total_kb(), pmm_free_kb(), kmalloc_used());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        int tot=pmm_total_kb(), fr=pmm_free_kb(), used=tot-fr;
        int bw=cw-24, fill=(tot?(used*bw)/tot:0);
        fb_fill_rect(cx+12,by,bw,16,rgb(200,200,200));
        fb_fill_rect(cx+12,by,fill,16,TH_ACCENT);
        fb_draw_rect(cx+12,by,bw,16,rgb(0,0,0)); by+=22;
        ksnprintf(b,sizeof(b),"ATA: %s (%d sett)  PCI: %d  Up: %d s",
            ata_present()?"OK":"n/d", ata_sectors(), pci_count(), uptime_sec());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        ksnprintf(b,sizeof(b),"NET: %s  Ora: %s", net_name(), ds);
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=18;
        ksnprintf(b,sizeof(b),"FS nodi: %d  Finestre: attiva", fs_count());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=20;
        fb_draw_string(cx+12,by,"PlexOS by NexonTech - QEMU + HW reale.",rgb(80,120,180));
    } else if(s->tab==1){
        char b[256], m[24];
        net_mac_str(m);
        ksnprintf(b,sizeof(b),"Scheda: %s", net_name());
        fb_draw_string(cx+12,by,b,TH_TITLE); by+=20;
        ksnprintf(b,sizeof(b),"Stato: %s  Link: %s  IP: %s",
            net_present()?"rilevata":"assente",
            net_link()?"SU":"GIU", net_ip());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=20;
        ksnprintf(b,sizeof(b),"MAC: %s  TX: %d  RX: %d", m, net_tx_count(), net_rx_count());
        fb_draw_string(cx+12,by,b,TH_TEXT); by+=22;
        /* pulsanti */
        const char* bn[3]={ net_enabled()?"[Disabilita]":"[Abilita]", "[Test TX]", "[DHCP?]" };
        int bx=cx+12;
        for(int i=0;i<3;i++){ fb_fill_rect(bx,by,110,26,TH_TITLE); fb_draw_string(bx+8,by+6,bn[i],rgb(255,255,255)); bx+=118; }
        by+=32;
        fb_draw_string(cx+12,by,"Driver Intel e1000 generico (82540EM).",rgb(80,80,80)); by+=18;
        fb_draw_string(cx+12,by,"QEMU: -net nic,model=e1000 -net user.",rgb(80,80,80)); by+=18;
        if(s->msg[0]){ fb_draw_string(cx+12,by,s->msg,rgb(0,140,0)); }
    } else {
        fb_draw_string(cx+12,by,"Tema desktop (anche da shell: theme 0/1/2):",TH_TITLE); by+=24;
        const char* tn[3]={"0 Scuro","1 Chiaro","2 Blu"};
        for(int i=0;i<3;i++){
            u32 bg=(gui_theme()==i)?TH_ACCENT:rgb(255,255,255);
            fb_fill_rect(cx+12,by,cw-24,28,bg);
            fb_draw_rect(cx+12,by,cw-24,28,TH_TITLE);
            fb_draw_string(cx+20,by+7,tn[i],TH_TEXT);
            by+=34;
        }
        fb_draw_string(cx+12,by,"Nuovi temi in arrivo (struttura pronta).",rgb(120,120,120));
    }
}
void app_sysmon_key(gui_window_t* w, int key, char ch){
    sys_t* s=(sys_t*)w->priv;
    if(!s->inited) s_init(s);
    UNUSED(key);
    if(ch=='1')s->tab=0; if(ch=='2')s->tab=1; if(ch=='3')s->tab=2;
    if(ch=='t') gui_set_theme((gui_theme()+1)%3);
    if(s->tab==1){
        if(ch=='e'||ch=='E'){ net_set_enabled(!net_enabled()); ksnprintf(s->msg,sizeof(s->msg),"Rete %s.", net_enabled()?"abilitata":"disabilitata"); }
        if(ch=='T'){ int r=net_tx_test(); ksnprintf(s->msg,sizeof(s->msg),r==0?"TX test OK.":"TX fallito."); }
    }
}
void app_sysmon_click(gui_window_t* w, int x, int y, int btn){
    sys_t* s=(sys_t*)w->priv;
    if(!s->inited) s_init(s);
    if(!(btn&1)) return;
    if(y>=6&&y<30){
        if(x>=8&&x<108){s->tab=0;return;}
        if(x>=114&&x<214){s->tab=1;return;}
        if(x>=220&&x<320){s->tab=2;return;}
    }
    if(s->tab==2){
        int by=6+36+24;
        for(int i=0;i<3;i++){
            if(y>=by&&y<by+28){ gui_set_theme(i); return; }
            by+=34;
        }
    }
    if(s->tab==1){
        /* pulsanti rete a y = 6+36+20+20+22 = circa 104 rel */
        int by=104;
        if(y>=by&&y<by+26){
            if(x>=12&&x<122){ net_set_enabled(!net_enabled()); ksnprintf(s->msg,sizeof(s->msg),"Rete %s.", net_enabled()?"abilitata":"disabilitata"); return; }
            if(x>=130&&x<240){ int r=net_tx_test(); ksnprintf(s->msg,sizeof(s->msg),r==0?"TX test OK, frame inviato.":"TX fallito (no link?)."); return; }
            if(x>=248&&x<358){ ksnprintf(s->msg,sizeof(s->msg),"IP: %s", net_ip()); return; }
        }
    }
}
