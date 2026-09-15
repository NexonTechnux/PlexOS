#include "plexos.h"

/* App 3: PlexCalc */

typedef struct { int inited; char expr[128]; char res[64]; } calc_t;

static void c_init(calc_t* c){ c->inited=1; c->expr[0]=0; strcpy(c->res,"0"); }

void app_calc_draw(gui_window_t* w){
    calc_t* c=(calc_t*)w->priv;
    if(!c->inited) c_init(c);
    int cx=w->x+6, cy=w->y+32, cw=w->w-12, ch=w->h-38;
    fb_fill_rect(cx,cy,cw,ch,rgb(240,244,250));
    /* display */
    fb_fill_rect(cx+10,cy+10,cw-20,56,rgb(15,20,35));
    fb_draw_rect(cx+10,cy+10,cw-20,56,TH_ACCENT);
    fb_draw_string(cx+20,cy+16,c->expr[0]?c->expr:"0",rgb(150,200,255));
    fb_draw_string(cx+20,cy+38,c->res,rgb(255,255,0));
    /* tastiera */
    const char* keys[5][4]={
        {"7","8","9","/"},
        {"4","5","6","*"},
        {"1","2","3","-"},
        {"0",".","=","+"},
        {"C","(",")","^"},
    };
    int bw=(cw-20-3*8)/4, bh=44;
    int sx=cx+10, sy=cy+76;
    for(int r=0;r<5;r++) for(int col=0;col<4;col++){
        int bx=sx+col*(bw+8), by=sy+r*(bh+8);
        if(bx+bw>cx+cw-10||by+bh>cy+ch-10) continue;
        const char* k=keys[r][col];
        u32 bg=rgb(255,255,255);
        if(k[0]=='=') bg=TH_ACCENT;
        if(k[0]=='C') bg=rgb(255,150,150);
        fb_fill_rect(bx,by,bw,bh,bg);
        fb_draw_rect(bx,by,bw,bh,TH_TITLE);
        fb_draw_string(bx+bw/2-4,by+bh/2-8,k,TH_TEXT);
    }
    fb_draw_string(cx+10,cy+ch-18,"PlexCalc: + - * / % ^ ( )",rgb(120,120,120));
}

static void c_press(calc_t* c, char k){
    if(k=='C'){ c->expr[0]=0; strcpy(c->res,"0"); return; }
    if(k=='='){
        if(!c->expr[0]) return;
        int ok=0; double v=calc_eval(c->expr,&ok);
        if(!ok) strcpy(c->res,"Errore");
        else ftoa_simple(v,c->res,4);
        return;
    }
    int l=strlen(c->expr);
    if(l<120){ c->expr[l]=k; c->expr[l+1]=0; }
}

void app_calc_key(gui_window_t* w, int key, char ch){
    calc_t* c=(calc_t*)w->priv;
    if(!c->inited) c_init(c);
    UNUSED(key);
    if(ch=='\b'){ int l=strlen(c->expr); if(l)c->expr[l-1]=0; return; }
    if(ch=='\n'){ c_press(c,'='); return; }
    if((ch>='0'&&ch<='9')||ch=='+'||ch=='-'||ch=='*'||ch=='/'||ch=='%'||ch=='^'||ch=='('||ch==')'||ch=='.')
        c_press(c,ch);
    if(ch=='c'||ch=='C') c_press(c,'C');
}
void app_calc_click(gui_window_t* w, int x, int y, int btn){
    calc_t* c=(calc_t*)w->priv;
    if(!c->inited) c_init(c);
    if(!(btn&1)) return;
    int cx=w->x+6, cy=w->y+32, cw=w->w-12;
    const char* keys[5][4]={ {"7","8","9","/"},{"4","5","6","*"},{"1","2","3","-"},{"0",".","=","+"},{"C","(",")","^"} };
    int bw=(cw-20-3*8)/4, bh=44;
    int sx=10, sy=76;
    for(int r=0;r<5;r++) for(int col=0;col<4;col++){
        int bx=sx+col*(bw+8), by=sy+r*(bh+8);
        if(x>=bx&&x<bx+bw&&y>=by&&y<by+bh){ c_press(c,keys[r][col][0]); return; }
    }
    UNUSED(cx); UNUSED(cy);
}
