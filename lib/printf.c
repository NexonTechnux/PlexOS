#include "plexos.h"

typedef __builtin_va_list va_list;
#define va_start(a,b) __builtin_va_start(a,b)
#define va_arg(a,t) __builtin_va_arg(a,t)
#define va_end(a) __builtin_va_end(a)

/* Emette stringa con padding: width, pad=' ' o '0' */
static void emit_pad(char** p, size_t* rem, size_t* tot, const char* s, int width, char pad){
    int len=strlen(s);
    int npad = width>len ? width-len : 0;
    /* se pad zero e numero negativo, il '-' deve stare prima: gestito dal chiamante? semplice: pad dopo '-' */
    if(pad=='0' && s[0]=='-'){
        /* emetti '-', poi zeri */
        if(*rem>1){ **p='-'; (*p)++; (*rem)--; } (*tot)++;
        s++; len--;
        npad = width>len+1 ? width-len-1 : 0;
        for(int i=0;i<npad;i++){ if(*rem>1){ **p='0'; (*p)++; (*rem)--; } (*tot)++; }
        for(;*s;s++){ if(*rem>1){ **p=*s; (*p)++; (*rem)--; } (*tot)++; }
        return;
    }
    for(int i=0;i<npad;i++){ if(*rem>1){ **p=pad; (*p)++; (*rem)--; } (*tot)++; }
    for(;*s;s++){ if(*rem>1){ **p=*s; (*p)++; (*rem)--; } (*tot)++; }
}

/* parsing: % [0] [width] spec */
static const char* parse_fmt(const char* f, int* width, char* pad){
    *width=0; *pad=' ';
    if(*f=='0'){ *pad='0'; f++; }
    while(*f>='0'&&*f<='9'){ *width=*width*10+(*f-'0'); f++; }
    return f;
}

int ksnprintf(char* buf, size_t n, const char* fmt, ...){
    va_list ap; va_start(ap, fmt);
    char* p=buf; size_t rem=n, tot=0;
    if(n==0) return 0;
    for(const char* f=fmt; *f; f++){
        if(*f!='%'){ if(rem>1){*p=*f;p++;rem--;} tot++; continue; }
        f++;
        int width=0; char pad=' ';
        f=parse_fmt(f,&width,&pad);
        if(*f=='d'||*f=='i'){ int v=va_arg(ap,int); char b[16]; itoa(v,b,10); emit_pad(&p,&rem,&tot,b,width,pad); }
        else if(*f=='u'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,10); emit_pad(&p,&rem,&tot,b,width,pad); }
        else if(*f=='x'||*f=='X'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,16); emit_pad(&p,&rem,&tot,b,width,pad); }
        else if(*f=='s'){ char* s=va_arg(ap,char*); if(!s)s="(null)"; emit_pad(&p,&rem,&tot,s,width,' '); }
        else if(*f=='c'){ char c=(char)va_arg(ap,int); char b[2]={c,0}; emit_pad(&p,&rem,&tot,b,width,' '); }
        else if(*f=='f'){ double d=va_arg(ap,double); char b[32]; ftoa_simple(d,b,3); emit_pad(&p,&rem,&tot,b,width,pad); }
        else if(*f=='%'){ if(rem>1){*p='%';p++;rem--;} tot++; }
        else if(*f=='p'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,16); if(rem>1){*p='0';p++;rem--;} tot++; if(rem>1){*p='x';p++;rem--;} tot++; emit_pad(&p,&rem,&tot,b,width,pad); }
        else { if(rem>1){*p='%';p++;rem--;} tot++; if(rem>1){*p=*f;p++;rem--;} tot++; }
    }
    *p=0;
    va_end(ap);
    return (int)tot;
}

int ksprintf(char* buf, const char* fmt, ...){
    va_list ap; va_start(ap, fmt);
    char* p=buf; size_t rem=1000000, tot=0;
    for(const char* f=fmt; *f; f++){
        if(*f!='%'){ *p++=*f; tot++; continue; }
        f++;
        int width=0; char pad=' ';
        f=parse_fmt(f,&width,&pad);
        if(*f=='d'||*f=='i'){ int v=va_arg(ap,int); char b[16]; itoa(v,b,10); /* pad manuale */
            int len=strlen(b), np=width>len?width-len:0;
            if(pad=='0'&&b[0]=='-'){*p++='-';tot++;for(int i=0;i<np;i++){*p++='0';tot++;}for(char*s=b+1;*s;s++){*p++=*s;tot++;}}
            else{for(int i=0;i<np;i++){*p++=pad;tot++;}for(char*s=b;*s;s++){*p++=*s;tot++;}} }
        else if(*f=='u'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,10); int len=strlen(b),np=width>len?width-len:0; for(int i=0;i<np;i++){*p++=pad;tot++;} for(char*s=b;*s;s++){*p++=*s;tot++;} }
        else if(*f=='x'||*f=='X'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,16); int len=strlen(b),np=width>len?width-len:0; for(int i=0;i<np;i++){*p++=pad;tot++;} for(char*s=b;*s;s++){*p++=*s;tot++;} }
        else if(*f=='s'){ char* s=va_arg(ap,char*); if(!s)s="(null)"; int len=strlen(s),np=width>len?width-len:0; for(int i=0;i<np;i++){*p++=' ';tot++;} for(;*s;s++){*p++=*s;tot++;} }
        else if(*f=='c'){ char c=(char)va_arg(ap,int); *p++=c;tot++; }
        else if(*f=='f'){ double d=va_arg(ap,double); char b[32]; ftoa_simple(d,b,3); int len=strlen(b),np=width>len?width-len:0; for(int i=0;i<np;i++){*p++=pad;tot++;} for(char*s=b;*s;s++){*p++=*s;tot++;} }
        else if(*f=='%'){ *p++='%';tot++; }
        else if(*f=='p'){ u32 v=va_arg(ap,u32); char b[16]; utoa(v,b,16); *p++='0';*p++='x';tot+=2; for(char*s=b;*s;s++){*p++=*s;tot++;} }
        else { *p++='%';*p++=*f;tot+=2; }
    }
    *p=0;
    va_end(ap);
    return (int)tot;
}

int kprintf(const char* fmt, ...){
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    char* p=buf;
    for(const char* f=fmt; *f; f++){
        if(*f!='%'){ if((size_t)(p-buf)<sizeof(buf)-1)*p++=*f; continue; }
        f++;
        int width=0; char pad=' ';
        f=parse_fmt(f,&width,&pad);
        char tmp[64]; tmp[0]=0;
        if(*f=='d'||*f=='i'){ int v=va_arg(ap,int); itoa(v,tmp,10); }
        else if(*f=='u'){ u32 v=va_arg(ap,u32); utoa(v,tmp,10); }
        else if(*f=='x'||*f=='X'){ u32 v=va_arg(ap,u32); utoa(v,tmp,16); }
        else if(*f=='s'){ char* s=va_arg(ap,char*); if(!s)s="(null)";
            int len=strlen(s),np=width>len?width-len:0;
            for(int i=0;i<np&&(size_t)(p-buf)<sizeof(buf)-1;i++)*p++=' ';
            for(;*s&&(size_t)(p-buf)<sizeof(buf)-1;s++)*p++=*s;
            continue; }
        else if(*f=='c'){ char c=(char)va_arg(ap,int); if((size_t)(p-buf)<sizeof(buf)-1)*p++=c; continue; }
        else if(*f=='f'){ double d=va_arg(ap,double); ftoa_simple(d,tmp,3); }
        else if(*f=='%'){ if((size_t)(p-buf)<sizeof(buf)-1)*p++='%'; continue; }
        else if(*f=='p'){ u32 v=va_arg(ap,u32); utoa(v,tmp,16);
            if((size_t)(p-buf)<sizeof(buf)-3){*p++='0';*p++='x';}
            for(char*s=tmp;*s&&(size_t)(p-buf)<sizeof(buf)-1;s++)*p++=*s;
            continue; }
        else { if((size_t)(p-buf)<sizeof(buf)-2){*p++='%';*p++=*f;} continue; }
        /* emetti tmp con padding */
        {
            int len=strlen(tmp),np=width>len?width-len:0;
            if(pad=='0'&&tmp[0]=='-'){
                if((size_t)(p-buf)<sizeof(buf)-1)*p++='-';
                for(int i=0;i<np&&(size_t)(p-buf)<sizeof(buf)-1;i++)*p++='0';
                for(char*s=tmp+1;*s&&(size_t)(p-buf)<sizeof(buf)-1;s++)*p++=*s;
            } else {
                for(int i=0;i<np&&(size_t)(p-buf)<sizeof(buf)-1;i++)*p++=pad;
                for(char*s=tmp;*s&&(size_t)(p-buf)<sizeof(buf)-1;s++)*p++=*s;
            }
        }
    }
    *p=0;
    va_end(ap);
    serial_write(buf);
    return (int)(p-buf);
}
