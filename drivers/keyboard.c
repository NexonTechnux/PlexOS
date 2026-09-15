#include "plexos.h"

/* Driver tastiera PS/2 QEMU (set 1), layout IT/US ibrido */

static char circ[512];
static int chead=0, ctail=0;
static int extbuf[128];
static int ehead=0, etail=0;
static int shift=0, ctrl=0, alt=0, caps=0, extended=0;

static const char normal_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static const char shift_map[128] = {
    0,27,'!','"','#','$','%','&','/','(',')','=','?','^','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','*','|',
    0,'?','Z','X','C','V','B','N','M',';',':','_',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void push_char(char c){
    int n=(chead+1)%(int)sizeof(circ);
    if(n!=ctail){ circ[chead]=c; chead=n; }
}
static void push_ext(int k){
    int n=(ehead+1)%128;
    if(n!=etail){ extbuf[ehead]=k; ehead=n; }
}

void keyboard_handler(regs_t* r){
    UNUSED(r);
    u8 sc = inb(0x60);
    if(sc==0xE0){ extended=1; return; }
    if(extended){
        extended=0;
        int rel = sc&0x80; u8 c=sc&0x7F;
        if(!rel){
            if(c==0x48) push_ext(KEY_UP);
            else if(c==0x50) push_ext(KEY_DOWN);
            else if(c==0x4B) push_ext(KEY_LEFT);
            else if(c==0x4D) push_ext(KEY_RIGHT);
            else if(c==0x47) push_ext(KEY_HOME);
            else if(c==0x4F) push_ext(KEY_END);
            else if(c==0x49) push_ext(KEY_UP);
            else if(c==0x51) push_ext(KEY_DOWN);
        }
        return;
    }
    int rel = sc&0x80; u8 code=sc&0x7F;
    if(code==0x2A||code==0x36){ shift=!rel; return; }
    if(code==0x1D){ ctrl=!rel; return; }
    if(code==0x38){ alt=!rel; return; }
    if(code==0x3A&&!rel){ caps=!caps; return; }
    if(rel) return;
    if(code==0x48) { push_ext(KEY_UP); return; }
    if(code==0x50) { push_ext(KEY_DOWN); return; }
    if(code==0x4B) { push_ext(KEY_LEFT); return; }
    if(code==0x4D) { push_ext(KEY_RIGHT); return; }
    if(code==0x47) { push_ext(KEY_HOME); return; }
    if(code==0x4F) { push_ext(KEY_END); return; }
    if(code==0x44) { push_ext(KEY_MENU); return; } /* F10 = Plex Start */
    if(code>=128) return;
    char c = (shift^caps) ? shift_map[code] : normal_map[code];
    if(code==0x1C) c='\n';
    if(code==0x39) c=' ';
    if(c) push_char(c);
    /* ctrl+C etc: passa comunque */
}

void keyboard_init(void){
    chead=ctail=ehead=etail=0;
    /* svuota buffer con timeout (su HW senza i8042 0xFF resta alto) */
    for(int i=0;i<1000;i++){ if(!(inb(0x64)&1)) break; inb(0x60); }
    /* abilita IRQ1 */
    u8 m = inb(0x21); outb(0x21, m & ~0x02);
    register_interrupt_handler(33, keyboard_handler);
    kprintf("[KBD] PS/2 inizializzata (QEMU i8042)\n");
}

int kbd_has_char(void){ return chead!=ctail; }
char kbd_getchar(void){
    if(chead==ctail) return 0;
    char c=circ[ctail]; ctail=(ctail+1)%(int)sizeof(circ);
    return c;
}
int kbd_has_key(void){ return ehead!=etail; }
int kbd_getkey(void){
    if(ehead==etail) return 0;
    int k=extbuf[etail]; etail=(etail+1)%128;
    return k;
}
