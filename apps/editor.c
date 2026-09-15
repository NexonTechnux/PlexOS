#include "plexos.h"

/* App 5: PlexEdit */

typedef struct {
    int inited;
    char path[256];
    char buf[4096];
    int len;
    int cursor;
    char status[160];
} ed_t;

static void e_init(ed_t* e){
    e->inited=1; strcpy(e->path,"/docs/nota.txt");
    e->len=0; e->cursor=0; e->buf[0]=0;
    strcpy(e->status,"Scrivi, poi Salva. Apri carica dal path.");
    fs_node_t* n=fs_resolve(e->path);
    if(n&&!n->is_dir&&n->data){ int l=n->size; if(l>4090)l=4090; memcpy(e->buf,n->data,l); e->buf[l]=0; e->len=l; e->cursor=l; }
}

void app_editor_draw(gui_window_t* w){
    ed_t* e=(ed_t*)w->priv;
    if(!e->inited) e_init(e);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(255,255,255));
    /* path bar */
    fb_fill_rect(cx,cy,cw,26,rgb(230,235,245));
    char pb[200]; ksnprintf(pb,sizeof(pb),"File: %s (%d ch)", e->path, e->len);
    fb_draw_string(cx+8,cy+6,pb,TH_TEXT);
    /* bottoni */
    int by=cy+30;
    const char* bn[4]={"[Salva]","[Apri]","[Pulisci]","[Docs]"};
    int bx=cx+8;
    for(int i=0;i<4;i++){ fb_fill_rect(bx,by,80,22,TH_TITLE); fb_draw_string(bx+6,by+5,bn[i],rgb(255,255,255)); bx+=86; }
    /* area testo */
    int ty=by+28, th=ch-28-30-26;
    fb_fill_rect(cx+8,ty,cw-16,th,rgb(255,255,255));
    fb_draw_rect(cx+8,ty,cw-16,th,rgb(150,150,150));
    int maxc=(cw-28)/8, maxr=(th-8)/16;
    /* mostra da scroll 0 (semplice) */
    int row=0, col=0, bi=0;
    /* calcola riga/col cursore */
    int crow=0, ccol=0;
    for(int i=0;i<e->cursor&&e->buf[i];i++){ if(e->buf[i]=='\n'){crow++;ccol=0;} else ccol++; }
    /* render */
    int ry=ty+4;
    char line[160]; int li=0;
    for(int i=0;i<=e->len&&row<maxr;i++){
        char c=(i<e->len)?e->buf[i]:0;
        if(c=='\n'||c==0){
            line[li]=0;
            /* tronca */
            if((int)strlen(line)>maxc) line[maxc]=0;
            fb_draw_string(cx+14,ry,line,TH_TEXT);
            ry+=16; row++; li=0;
            if(c==0) break;
        } else if(li<150){ line[li++]=c; }
        UNUSED(col); UNUSED(bi);
    }
    /* cursore */
    if(w->focused&&((timer_ticks()/30)%2==0)){
        int ccx=cx+14+ccol*8, ccy=ty+4+crow*16;
        if(ccx<cx+cw-20&&ccy<ty+th-10) fb_fill_rect(ccx,ccy,8,16,TH_ACCENT);
    }
    /* status */
    fb_fill_rect(cx,cy+ch-22,cw,22,rgb(230,235,245));
    fb_draw_string(cx+8,cy+ch-17,e->status,rgb(80,80,80));
}

void app_editor_key(gui_window_t* w, int key, char ch){
    ed_t* e=(ed_t*)w->priv;
    if(!e->inited) e_init(e);
    if(key==KEY_LEFT){ if(e->cursor>0)e->cursor--; return; }
    if(key==KEY_RIGHT){ if(e->cursor<e->len)e->cursor++; return; }
    if(key==KEY_HOME){ while(e->cursor>0&&e->buf[e->cursor-1]!='\n')e->cursor--; return; }
    if(key==KEY_END){ while(e->cursor<e->len&&e->buf[e->cursor]!='\n')e->cursor++; return; }
    if(ch=='\b'){ if(e->cursor>0){ memmove(e->buf+e->cursor-1,e->buf+e->cursor,e->len-e->cursor+1); e->cursor--; e->len--; } return; }
    if(ch>=32&&ch<127||ch=='\n'){
        if(e->len<4090){ memmove(e->buf+e->cursor+1,e->buf+e->cursor,e->len-e->cursor+1); e->buf[e->cursor]=ch; e->cursor++; e->len++; }
    }
}
void app_editor_click(gui_window_t* w, int x, int y, int btn){
    ed_t* e=(ed_t*)w->priv;
    if(!e->inited) e_init(e);
    if(!(btn&1)) return;
    /* bottoni y 30..52 */
    if(y>=30&&y<52){
        int bx=8;
        if(x>=bx&&x<bx+80){ /* salva */
            fs_touch(e->path);
            if(fs_write_file(e->path,e->buf,e->len)==0) strcpy(e->status,"Salvato.");
            else strcpy(e->status,"Errore salvataggio.");
            return;
        }
        bx+=86;
        if(x>=bx&&x<bx+80){ /* apri */
            fs_node_t* n=fs_resolve(e->path);
            if(n&&!n->is_dir&&n->data){ int l=n->size; if(l>4090)l=4090; memcpy(e->buf,n->data,l); e->buf[l]=0; e->len=l; e->cursor=l; strcpy(e->status,"Caricato."); }
            else strcpy(e->status,"File non trovato.");
            return;
        }
        bx+=86;
        if(x>=bx&&x<bx+80){ e->len=0;e->cursor=0;e->buf[0]=0; strcpy(e->status,"Pulito."); return; }
        bx+=86;
        if(x>=bx&&x<bx+80){ strcpy(e->path,"/docs/benvenuto.txt"); fs_node_t* n=fs_resolve(e->path); if(n&&n->data){int l=n->size; if(l>4090)l=4090; memcpy(e->buf,n->data,l); e->buf[l]=0; e->len=l; e->cursor=l;} strcpy(e->status,"Docs caricato."); return; }
    }
}
