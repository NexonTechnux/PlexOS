#include "plexos.h"

/* PlexCalc: motore espressioni */

static const char* g_p;
static int g_err;

static void skip_sp(void){ while(*g_p==' '||*g_p=='\t') g_p++; }
static double parse_expr(void);
static double parse_term(void);
static double parse_fact(void);
static double parse_pow(void);
static double parse_prim(void);

static double parse_prim(void){
    skip_sp();
    if(*g_p=='('){ g_p++; double v=parse_expr(); skip_sp(); if(*g_p==')')g_p++; else g_err=1; return v; }
    if(*g_p=='-'){ g_p++; return -parse_prim(); }
    if(*g_p=='+'){ g_p++; return parse_prim(); }
    /* numero */
    if((*g_p>='0'&&*g_p<='9')||*g_p=='.'){
        double v=0;
        while(*g_p>='0'&&*g_p<='9'){v=v*10+(*g_p-'0');g_p++;}
        if(*g_p=='.'){g_p++;double f=0.1;while(*g_p>='0'&&*g_p<='9'){v+=(*g_p-'0')*f;f*=0.1;g_p++;}}
        return v;
    }
    g_err=1; return 0;
}
static double my_pow(double a,double b){
    /* b intera? */
    if(b<0) return 1/my_pow(a,-b);
    long n=(long)b;
    if((double)n!=b){
        /* approssimazione: non supportiamo radici frazionarie precise, usa ripetuta */
        g_err=1; return 0;
    }
    double r=1; for(long i=0;i<n;i++) r*=a; return r;
}
static double parse_pow(void){
    double a=parse_prim(); skip_sp();
    if(*g_p=='^'){ g_p++; double b=parse_pow(); return my_pow(a,b); }
    return a;
}
static double parse_fact(void){
    double a=parse_pow();
    while(1){ skip_sp(); if(*g_p=='*'||*g_p=='/'||*g_p=='%'){
        char op=*g_p++; double b=parse_pow();
        if(op=='*')a*=b; else if(op=='/'){ if(b==0){g_err=1;return 0;} a/=b; }
        else { long x=(long)a,y=(long)b; if(y==0){g_err=1;return 0;} a=x%y; }
    } else break; }
    return a;
}
static double parse_term(void){ return parse_fact(); }
static double parse_expr(void){
    double a=parse_term();
    while(1){ skip_sp(); if(*g_p=='+'||*g_p=='-'){ char op=*g_p++; double b=parse_term(); if(op=='+')a+=b; else a-=b; } else break; }
    return a;
}

double calc_eval(const char* expr, int* ok){
    g_p=expr; g_err=0;
    double v=parse_expr();
    skip_sp();
    if(*g_p!=0) g_err=1;
    if(ok) *ok=!g_err;
    return v;
}
