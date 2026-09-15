#include "plexos.h"

/* App 2: PlexFiles */

typedef struct {
    int inited;
    char path[256];
    int sel;
    char status[160];
    int counter;
} fm_t;

static void fm_init(fm_t* f){
    f->inited=1; strcpy(f->path,"/"); f->sel=0;
    strcpy(f->status,"Pronto. Click per selezionare, doppio-click=Apri.");
    f->counter=0;
}

void app_fileman_draw(gui_window_t* w){
    fm_t* f=(fm_t*)w->priv;
    if(!f->inited) fm_init(f);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(255,255,255));
    /* barra path */
    fb_fill_rect(cx,cy,cw,26,rgb(230,235,245));
    char pb[256]; ksnprintf(pb,sizeof(pb),"Percorso: %s", f->path);
    fb_draw_string(cx+8,cy+6,pb,TH_TEXT);
    /* pulsanti */
    const char* btns[5]={"[Su]","[+Cart]","[+File]","[Del]","[Apri]"};
    int bx=cx+8, by=cy+30;
    for(int i=0;i<5;i++){
        fb_fill_rect(bx,by,86,22,TH_TITLE);
        fb_draw_string(bx+6,by+4,btns[i],rgb(255,255,255));
        bx+=92;
    }
    /* lista */
    fs_node_t* d=fs_resolve(f->path);
    int ly=by+30;
    if(!d||!d->is_dir){
        fb_draw_string(cx+8,ly,"(cartella non valida)",rgb(200,0,0));
    } else {
        int idx=0;
        /* conta */
        int total=0; for(fs_node_t* c=d->child;c;c=c->next) total++;
        if(f->sel>=total&&total>0) f->sel=total-1;
        if(f->sel<0)f->sel=0;
        for(fs_node_t* c=d->child;c;c=c->next){
            if(ly+18>cy+ch-26) break;
            u32 bg=(idx==f->sel)?TH_ACCENT:rgb(255,255,255);
            u32 fg=(idx==f->sel)?rgb(255,255,255):TH_TEXT;
            fb_fill_rect(cx+8,ly,cw-16,20,bg);
            char b[140];
            if(c->is_dir) ksnprintf(b,sizeof(b),"[DIR]  %s/", c->name);
            else ksnprintf(b,sizeof(b),"[FILE] %s (%d B)", c->name, c->size);
            fb_draw_string(cx+14,ly+3,b,fg);
            ly+=22; idx++;
        }
        if(total==0) fb_draw_string(cx+8,ly,"(cartella vuota)",rgb(130,130,130));
    }
    /* status bar */
    fb_fill_rect(cx,cy+ch-22,cw,22,rgb(230,235,245));
    fb_draw_string(cx+8,cy+ch-17,f->status,rgb(80,80,80));
}

static fs_node_t* fm_selected(fm_t* f){
    fs_node_t* d=fs_resolve(f->path);
    if(!d) return NULL;
    int i=0;
    for(fs_node_t* c=d->child;c;c=c->next){ if(i==f->sel) return c; i++; }
    return NULL;
}

void app_fileman_key(gui_window_t* w, int key, char ch){
    fm_t* f=(fm_t*)w->priv;
    if(!f->inited) fm_init(f);
    if(key==KEY_UP){ f->sel--; if(f->sel<0)f->sel=0; }
    else if(key==KEY_DOWN){ f->sel++; }
    else if(ch=='\n'){
        fs_node_t* s=fm_selected(f);
        if(s&&s->is_dir){
            char nb[256];
            if(strcmp(f->path,"/")==0) ksnprintf(nb,sizeof(nb),"/%s",s->name);
            else ksnprintf(nb,sizeof(nb),"%s/%s",f->path,s->name);
            strcpy(f->path,nb); f->sel=0;
            ksnprintf(f->status,sizeof(f->status),"Aperto %s",nb);
        } else if(s){ ksnprintf(f->status,sizeof(f->status),"File: %s (%d bytes) - usa Editor", s->name, s->size); }
    }
    else if(ch=='\b'){
        if(strcmp(f->path,"/")!=0){
            /* vai su: tronca ultimo componente */
            int len=strlen(f->path);
            while(len>0&&f->path[len-1]=='/')len--;
            while(len>0&&f->path[len-1]!='/')len--;
            if(len<=1) strcpy(f->path,"/");
            else { f->path[len-1]=0; if(!f->path[0])strcpy(f->path,"/"); }
            f->sel=0; strcpy(f->status,"Livello superiore.");
        }
    }
}

void app_fileman_click(gui_window_t* w, int x, int y, int btn){
    fm_t* f=(fm_t*)w->priv;
    if(!f->inited) fm_init(f);
    if(!(btn&1)) return;
    int cx=w->x+6, cw=w->w-12;
    UNUSED(cx); UNUSED(cw); UNUSED(w);
    /* pulsanti: y relativo a contenuto (0..) */
    if(y>=30&&y<52){
        if(x>=8&&x<94){ /* Su */
            app_fileman_key(w,0,'\b');
        } else if(x>=100&&x<186){ /* +Cart */
            char nb[256]; f->counter++;
            ksnprintf(nb,sizeof(nb),"%s%snuova_cartella_%d", f->path, strcmp(f->path,"/")==0?"":"/", f->counter);
            if(fs_mkdir(nb)==0) strcpy(f->status,"Cartella creata.");
            else strcpy(f->status,"Impossibile creare.");
        } else if(x>=192&&x<278){ /* +File */
            char nb[256]; f->counter++;
            ksnprintf(nb,sizeof(nb),"%s%snuovo_file_%d.txt", f->path, strcmp(f->path,"/")==0?"":"/", f->counter);
            if(fs_touch(nb)==0){ fs_write_file(nb,"Nuovo file PlexOS\n",17); strcpy(f->status,"File creato."); }
            else strcpy(f->status,"Impossibile creare file.");
        } else if(x>=284&&x<370){ /* Del */
            fs_node_t* s=fm_selected(f);
            if(s){
                char full[256];
                ksnprintf(full,sizeof(full),"%s%s%s", f->path, strcmp(f->path,"/")==0?"":"/", s->name);
                int r=fs_rm(full);
                if(r==0) strcpy(f->status,"Eliminato.");
                else if(r==-2) strcpy(f->status,"Cartella non vuota.");
                else strcpy(f->status,"Errore eliminazione.");
            }
        } else if(x>=376&&x<462){ /* Apri */
            app_fileman_key(w,0,'\n');
        }
        return;
    }
    if(y>=60){
        int idx=(y-60)/22;
        f->sel=idx;
        /* doppio click simulato: se click sullo stesso entro breve? semplifichiamo: click seleziona */
        fs_node_t* s=fm_selected(f);
        if(s&&s->is_dir){
            /* se click 2 volte veloci non tracciato; usa tasto Apri o Invio */
            char b[160]; ksnprintf(b,sizeof(b),"Selezionato: %s - premi Apri/Invio", s->name);
            strcpy(f->status,b);
        } else if(s){
            char b[160]; ksnprintf(b,sizeof(b),"File: %s (%d B)", s->name, s->size);
            strcpy(f->status,b);
        }
    }
}
