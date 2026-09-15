#include "plexos.h"

/* Heap bump + free-list semplice, ~8MB statica */
#define HEAP_SIZE (8*1024*1024)
static u8 heap_area[HEAP_SIZE];
static u32 heap_brk = 0;
static u32 heap_used_peak = 0;

typedef struct block { u32 size; u8 free; struct block* next; } block_t;
static block_t* head = NULL;

void* kmalloc(size_t size){
    if(size==0) size=1;
    size = (size+7)&~7;
    /* cerca blocco libero */
    block_t* b=head;
    while(b){
        if(b->free && b->size>=size){ b->free=0; return (void*)(b+1); }
        b=b->next;
    }
    u32 need = sizeof(block_t)+size;
    if(heap_brk+need > HEAP_SIZE) { kprintf("[KMALLOC] OOM richiesto=%d\n", size); return NULL; }
    b = (block_t*)(heap_area+heap_brk);
    heap_brk += need;
    b->size=size; b->free=0; b->next=NULL;
    if(!head) head=b;
    else { block_t* t=head; while(t->next)t=t->next; t->next=b; }
    heap_used_peak += need;
    memset(b+1,0,size);
    return (void*)(b+1);
}
void* kcalloc(size_t n, size_t sz){ size_t t=n*sz; void* p=kmalloc(t); if(p) memset(p,0,t); return p; }
void kfree(void* p){
    if(!p) return;
    block_t* b=((block_t*)p)-1;
    /* sanity: dentro heap? */
    u32 off=(u32)((u8*)b-heap_area);
    if(off>=HEAP_SIZE) return;
    b->free=1;
    memset(p,0,b->size);
}
void* krealloc(void* p, size_t ns){
    if(!p) return kmalloc(ns);
    if(ns==0){kfree(p);return NULL;}
    block_t* b=((block_t*)p)-1;
    if(b->size>=ns) return p;
    void* n=kmalloc(ns);
    if(!n) return NULL;
    memcpy(n,p,b->size);
    kfree(p);
    return n;
}
u32 kmalloc_used(void){
    u32 used=0; block_t* b=head;
    while(b){ if(!b->free) used+=b->size+sizeof(block_t); b=b->next; }
    return used;
}
char* kstrdup(const char* s){
    size_t l=strlen(s)+1; char* p=(char*)kmalloc(l);
    if(p) memcpy(p,s,l);
    return p;
}
