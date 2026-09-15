#include "plexos.h"

size_t strlen(const char* s) { size_t i=0; while(s[i]) i++; return i; }
int strcmp(const char* a, const char* b) {
    while(*a && *a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b;
}
int strncmp(const char* a, const char* b, size_t n) {
    for(size_t i=0;i<n;i++){ if(a[i]!=b[i]) return (unsigned char)a[i]-(unsigned char)b[i]; if(!a[i]) return 0; } return 0;
}
char* strcpy(char* d, const char* s){ char* o=d; while((*d++=*s++)); return o; }
char* strncpy(char* d, const char* s, size_t n){ size_t i=0; for(;i<n&&s[i];i++) d[i]=s[i]; for(;i<n;i++) d[i]=0; return d; }
char* strcat(char* d, const char* s){ char* o=d; while(*d)d++; while((*d++=*s++)); return o; }
char* strchr(const char* s, int c){ while(*s){ if(*s==c) return (char*)s; s++; } return c==0?(char*)s:NULL; }
char* strstr(const char* h, const char* n){
    if(!*n) return (char*)h;
    for(;*h;h++){ const char* a=h,*b=n; while(*a&&*b&&*a==*b){a++;b++;} if(!*b) return (char*)h; }
    return NULL;
}
int memcmp(const void* a, const void* b, size_t n){ const u8* x=a,*y=b; for(size_t i=0;i<n;i++) if(x[i]!=y[i]) return x[i]-y[i]; return 0; }
void* memcpy(void* d, const void* s, size_t n){ u8* a=d; const u8* b=s; for(size_t i=0;i<n;i++) a[i]=b[i]; return d; }
void* memmove(void* d, const void* s, size_t n){ u8* a=d; const u8* b=s; if(a<b) for(size_t i=0;i<n;i++) a[i]=b[i]; else for(size_t i=n;i>0;i--) a[i-1]=b[i-1]; return d; }
void* memset(void* s, int c, size_t n){ u8* a=s; for(size_t i=0;i<n;i++) a[i]=(u8)c; return s; }

int toupper_c(int c){ if(c>='a'&&c<='z') return c-32; return c; }
int tolower_c(int c){ if(c>='A'&&c<='Z') return c+32; return c; }
int isdigit_c(int c){ return c>='0'&&c<='9'; }
int isalpha_c(int c){ return (c>='A'&&c<='Z')||(c>='a'&&c<='z'); }
int isspace_c(int c){ return c==' '||c=='\t'||c=='\n'||c=='\r'; }

int atoi(const char* s){ int neg=0,v=0; while(isspace_c(*s))s++; if(*s=='-'){neg=1;s++;}else if(*s=='+')s++; while(isdigit_c(*s)){v=v*10+(*s-'0');s++;} return neg?-v:v; }
u32 atou(const char* s){ u32 v=0; if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){ s+=2; while(1){ char c=*s; int d=-1; if(c>='0'&&c<='9')d=c-'0'; else if(c>='a'&&c<='f')d=c-'a'+10; else if(c>='A'&&c<='F')d=c-'A'+10; else break; v=v*16+d; s++; } return v;} while(isdigit_c(*s)){v=v*10+(*s-'0');s++;} return v; }

void itoa(int v, char* buf, int base){
    const char* d="0123456789ABCDEF"; char tmp[33]; int i=0,neg=0;
    if(base==10&&v<0){neg=1;v=-v;}
    if(v==0) tmp[i++]='0';
    unsigned int u=(unsigned int)v;
    while(u){tmp[i++]=d[u%base];u/=base;}
    int j=0; if(neg)buf[j++]='-';
    while(i--)buf[j++]=tmp[i]; buf[j]=0;
}
void utoa(u32 v, char* buf, int base){
    const char* d="0123456789ABCDEF"; char tmp[33]; int i=0;
    if(v==0)tmp[i++]='0';
    while(v){tmp[i++]=d[v%base];v/=base;}
    int j=0; while(i--)buf[j++]=tmp[i]; buf[j]=0;
}

double atof_simple(const char* s){
    double v=0; int neg=0;
    while(isspace_c(*s))s++;
    if(*s=='-'){neg=1;s++;}else if(*s=='+')s++;
    while(isdigit_c(*s)){v=v*10+(*s-'0');s++;}
    if(*s=='.'){s++;double f=0.1;while(isdigit_c(*s)){v+=(*s-'0')*f;f*=0.1;s++;}}
    return neg?-v:v;
}
void ftoa_simple(double v, char* buf, int prec){
    if(prec<0)prec=4; if(prec>8)prec=8;
    int i=0;
    if(v<0){buf[i++]='-';v=-v;}
    long ip=(long)v; double fp=v-ip;
    char tmp[24]; int ti=0;
    if(ip==0)tmp[ti++]='0';
    while(ip){tmp[ti++]='0'+ip%10;ip/=10;}
    while(ti--)buf[i++]=tmp[ti];
    if(prec>0){buf[i++]='.';for(int k=0;k<prec;k++){fp*=10;int d=(int)fp;buf[i++]='0'+d;fp-=d;}}
    buf[i]=0;
}
