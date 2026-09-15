#include "plexos.h"

/* App 1: PlexTerm */

typedef struct {
    int inited;
    char lines[48][128];
    int nlines;
    char input[256];
    int ilen;
    char hist[16][256];
    int hcount, hpos;
} term_t;

static void t_push(term_t* t, const char* s){
    /* spezza su \n */
    char tmp[256]; int ti=0;
    for(int i=0;;i++){
        char c=s[i];
        if(c=='\n'||c==0){
            tmp[ti]=0;
            if(t->nlines<48){ strcpy(t->lines[t->nlines++],tmp); }
            else { for(int k=0;k<47;k++) strcpy(t->lines[k],t->lines[k+1]); strcpy(t->lines[47],tmp); }
            ti=0;
            if(c==0) break;
        } else if(ti<120){ tmp[ti++]=c; }
    }
}

static void t_init(term_t* t){
    t->inited=1; t->nlines=0; t->ilen=0; t->hcount=0; t->hpos=0;
    t_push(t,"PlexOS 1.0 - PlexTerm 720p");
    t_push(t,"Scrivi 'help' per i comandi.");
    t_push(t,"");
}

void app_terminal_draw(gui_window_t* w){
    term_t* t=(term_t*)w->priv;
    if(!t->inited) t_init(t);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(10,12,20));
    int rows=(ch-8)/16;
    int start=t->nlines-rows+1; /* +1 per input */
    if(start<0)start=0;
    u32 fg=rgb(200,255,200);
    for(int i=0;i<rows-1;i++){
        int li=start+i;
        if(li<t->nlines) fb_draw_string(cx+6,cy+4+i*16,t->lines[li],fg);
    }
    /* prompt input */
    char cwd[64]; fs_cwd_str(cwd,sizeof(cwd));
    char prompt[320];
    ksnprintf(prompt,sizeof(prompt),"user@plexos:%s$ %s", cwd, t->input);
    prompt[60]=0; /* evita overflow visivo: mostra coda */
    /* se troppo lungo mostra solo coda */
    int plen=strlen(prompt);
    const char* show=prompt;
    int maxc=(cw-12)/8;
    if(plen>maxc) show=prompt+plen-maxc;
    int iy=cy+ch-20;
    fb_draw_string(cx+6,iy,show,rgb(255,255,0));
    /* cursore */
    if(w->focused && ((timer_ticks()/30)%2==0)){
        int cxx=cx+6+strlen(show)*8;
        fb_fill_rect(cxx,iy,8,16,rgb(255,255,0));
    }
}

void app_terminal_key(gui_window_t* w, int key, char ch){
    term_t* t=(term_t*)w->priv;
    if(!t->inited) t_init(t);
    if(key==KEY_UP){
        if(t->hcount>0){ if(t->hpos>0)t->hpos--; strcpy(t->input,t->hist[t->hpos]); t->ilen=strlen(t->input); }
        return;
    }
    if(key==KEY_DOWN){
        if(t->hcount>0){ if(t->hpos<t->hcount-1){t->hpos++; strcpy(t->input,t->hist[t->hpos]);} else {t->input[0]=0;t->ilen=0;t->hpos=t->hcount;} t->ilen=strlen(t->input); }
        return;
    }
    if(ch=='\b'){ if(t->ilen>0){t->ilen--; t->input[t->ilen]=0;} return; }
    if(ch=='\n'){
        char line[300];
        char cwd[64]; fs_cwd_str(cwd,sizeof(cwd));
        ksnprintf(line,sizeof(line),"user@plexos:%s$ %s",cwd,t->input);
        t_push(t,line);
        if(t->ilen>0 && (t->hcount==0||strcmp(t->hist[t->hcount-1],t->input)!=0)){
            if(t->hcount<16){ strcpy(t->hist[t->hcount++],t->input); }
            else { for(int i=0;i<15;i++) strcpy(t->hist[i],t->hist[i+1]); strcpy(t->hist[15],t->input); }
        }
        t->hpos=t->hcount;
        char out[2048]; int clr=0, ex=0;
        shell_execute(t->input,out,sizeof(out),&clr,&ex);
        if(clr){ t->nlines=0; }
        if(out[0]) t_push(t,out);
        t->ilen=0; t->input[0]=0;
        if(ex){ gui_close(w); }
        return;
    }
    if(ch>=32&&ch<127){
        if(t->ilen<250){ t->input[t->ilen++]=ch; t->input[t->ilen]=0; }
    }
}
void app_terminal_click(gui_window_t* w, int x, int y, int btn){ UNUSED(w);UNUSED(x);UNUSED(y);UNUSED(btn); }
