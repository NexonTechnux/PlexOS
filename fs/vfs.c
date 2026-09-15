#include "plexos.h"

static fs_node_t* root = NULL;
static fs_node_t* cwd = NULL;
static int node_count = 0;

static fs_node_t* new_node(const char* name, int is_dir, fs_node_t* parent){
    fs_node_t* n = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if(!n) return NULL;
    memset(n,0,sizeof(*n));
    strncpy(n->name,name,63);
    n->is_dir=is_dir; n->parent=parent?parent:n;
    node_count++;
    return n;
}
static void attach(fs_node_t* parent, fs_node_t* n){
    n->next=parent->child; parent->child=n; n->parent=parent;
}
static fs_node_t* find_child(fs_node_t* dir, const char* name){
    if(!dir||!dir->is_dir) return NULL;
    for(fs_node_t* c=dir->child;c;c=c->next)
        if(strcmp(c->name,name)==0) return c;
    return NULL;
}

static void split_parent(const char* path, char* dirbuf, char* base){
    /* dirbuf deve contenere 256, base 64 */
    int len=strlen(path);
    while(len>1 && path[len-1]=='/') len--;
    int slash=-1;
    for(int i=0;i<len;i++) if(path[i]=='/') slash=i;
    if(slash<=0){ strcpy(dirbuf,"/"); if(slash==0){ strncpy(base,path+1,63);} else {strncpy(base,path,63);} }
    else { strncpy(dirbuf,path,slash); dirbuf[slash]=0; strncpy(base,path+slash+1,63); }
    dirbuf[255]=0; base[63]=0;
    /* pulisci base da trailing slash */
    int bl=strlen(base); while(bl>0&&base[bl-1]=='/'){base[bl-1]=0;bl--;}
}

fs_node_t* fs_resolve_from(fs_node_t* base, const char* path){
    if(!path||!*path) return base;
    fs_node_t* cur;
    if(path[0]=='/') cur=root;
    else cur=base?base:cwd;
    char tmp[256]; strncpy(tmp,path,255); tmp[255]=0;
    /* tokenizza */
    char* p=tmp;
    char tok[64]; int ti=0;
    /* gestisci path intero */
    for(int i=0;;i++){
        char c=tmp[i];
        if(c=='/'||c==0){
            if(ti>0){
                tok[ti]=0; ti=0;
                if(strcmp(tok,".")==0){}
                else if(strcmp(tok,"..")==0){ if(cur->parent) cur=cur->parent; }
                else { fs_node_t* n=find_child(cur,tok); if(!n) return NULL; cur=n; }
            }
            if(c==0) break;
        } else {
            if(ti<63) tok[ti++]=c;
        }
    }
    UNUSED(p);
    return cur;
}
fs_node_t* fs_resolve(const char* path){ return fs_resolve_from(cwd,path); }
fs_node_t* fs_root(void){ return root; }
fs_node_t* fs_cwd(void){ return cwd; }
int fs_is_dir(fs_node_t* n){ return n&&n->is_dir; }
int fs_count(void){ return node_count; }

void fs_cwd_str(char* buf, size_t n){
    if(cwd==root){ strncpy(buf,"/",n); return; }
    /* costruisci al contrario */
    char parts[16][64]; int depth=0;
    fs_node_t* c=cwd;
    while(c&&c!=root&&depth<16){ strncpy(parts[depth],c->name,63); depth++; c=c->parent; }
    buf[0]=0;
    for(int i=depth-1;i>=0;i--){ strcat(buf,"/"); strcat(buf,parts[i]); }
    if(!buf[0]) strcpy(buf,"/");
}
void fs_set_cwd(const char* path){
    fs_node_t* n=fs_resolve(path);
    if(n&&n->is_dir) cwd=n;
}

int fs_mkdir(const char* path){
    if(!path||!*path) return -1;
    char dir[256], base[64];
    /* se path senza slash e relativo -> crea in cwd */
    if(!strchr(path,'/')){
        if(find_child(cwd,path)) return -1;
        fs_node_t* n=new_node(path,1,cwd);
        if(!n) return -1;
        attach(cwd,n); return 0;
    }
    split_parent(path,dir,base);
    if(!base[0]) return -1;
    fs_node_t* d=fs_resolve(dir);
    if(!d||!d->is_dir) return -1;
    if(find_child(d,base)) return -1;
    fs_node_t* n=new_node(base,1,d);
    if(!n) return -1;
    attach(d,n); return 0;
}
int fs_touch(const char* path){
    if(!path||!*path) return -1;
    fs_node_t* e=fs_resolve(path);
    if(e) return 0;
    char dir[256], base[64];
    if(!strchr(path,'/')){
        fs_node_t* n=new_node(path,0,cwd);
        if(!n) return -1;
        attach(cwd,n); return 0;
    }
    split_parent(path,dir,base);
    if(!base[0]) return -1;
    fs_node_t* d=fs_resolve(dir);
    if(!d||!d->is_dir) return -1;
    fs_node_t* n=new_node(base,0,d);
    if(!n) return -1;
    attach(d,n); return 0;
}
int fs_write_file(const char* path, const char* data, u32 len){
    fs_node_t* n=fs_resolve(path);
    if(!n){ if(fs_touch(path)!=0) return -1; n=fs_resolve(path); if(!n) return -1; }
    if(n->is_dir) return -1;
    if(n->cap < len+1){
        if(n->data) kfree(n->data);
        n->data=(char*)kmalloc(len+1);
        if(!n->data){n->cap=0;n->size=0;return -1;}
        n->cap=len+1;
    }
    memcpy(n->data,data,len); n->data[len]=0; n->size=len;
    return 0;
}
int fs_append_file(const char* path, const char* data, u32 len){
    fs_node_t* n=fs_resolve(path);
    if(!n){ return fs_write_file(path,data,len); }
    if(n->is_dir) return -1;
    u32 nl=n->size+len;
    char* nd=(char*)kmalloc(nl+1);
    if(!nd) return -1;
    if(n->data){ memcpy(nd,n->data,n->size); kfree(n->data); }
    memcpy(nd+n->size,data,len); nd[nl]=0;
    n->data=nd; n->size=nl; n->cap=nl+1;
    return 0;
}
int fs_rm(const char* path){
    if(!path||strcmp(path,"/")==0) return -1;
    fs_node_t* n=fs_resolve(path);
    if(!n||n==root||n==cwd) return -1;
    if(n->is_dir && n->child) return -2; /* non vuota */
    fs_node_t* p=n->parent;
    fs_node_t** link=&p->child;
    while(*link && *link!=n) link=&(*link)->next;
    if(*link) *link=n->next;
    if(n->data) kfree(n->data);
    kfree(n); node_count--;
    return 0;
}
int fs_list_names(fs_node_t* dir, char names[][64], int maxn){
    if(!dir||!dir->is_dir) return 0;
    int c=0;
    for(fs_node_t* n=dir->child;n&&c<maxn;n=n->next){ strncpy(names[c],n->name,63); c++; }
    return c;
}

static void seed_file(const char* path, const char* content){
    fs_touch(path);
    fs_write_file(path,content,strlen(content));
}

void fs_init(void){
    root=new_node("/",1,NULL); root->parent=root;
    cwd=root; node_count=1;
    fs_mkdir("/apps"); fs_mkdir("/docs"); fs_mkdir("/sys"); fs_mkdir("/home"); fs_mkdir("/home/user");
    seed_file("/docs/benvenuto.txt",
        "Benvenuto in PlexOS 1.0!\n"
        "=======================\n"
        "Kernel 720p + GUI + 6 app.\n"
        "Prova 'help' nel terminale.\n"
        "Cartelle: /apps /docs /sys /home/user\n");
    seed_file("/docs/guida.txt",
        "GUIDA PlexOS (NexonTech)\n"
        "File: ls cd pwd mkdir touch cat echo write rm cp mv\n"
        "      tree find grep head tail wc du df stat mount open\n"
        "Sistema: sysinfo meminfo free cpuinfo lscpu gpu pci vinfo\n"
        "         netinfo netsend netup netdown netstat ps uptime\n"
        "         date cal calc theme dmesg history sleep fortune\n"
        "         apps plexfetch reboot shutdown\n"
        "App: Terminale, File Manager, Calcolatrice, Browser,\n"
        "     Editor, Impostazioni (F10 = Plex Start)\n");
    seed_file("/docs/qemu.txt",
        "Driver e supporto PlexOS (NexonTech):\n"
        "- Framebuffer 1280x720x32 @60fps dbl-buf+vsync\n"
        "- Tastiera i8042 + Mouse PS/2\n"
        "- Timer PIT 100Hz\n"
        "- Seriale COM1 / 0xE9 debug\n"
        "- PCI bus 0-7 (QEMU + HW reale)\n"
        "- CPU Intel Core i3-1115G4 ready (CPUID)\n"
        "- GPU Intel UHD Graphics G4 ready\n"
        "- Rete Intel e1000 generica (MAC reale)\n"
        "- ATA IDE PIO\n"
        "- RTC CMOS data/ora\n");
    seed_file("/sys/version","PlexOS 1.0.0 (kernel 720p, build QEMU)\n");
    kprintf("[FS] VFS pronta (nodi=%d)\n", node_count);
}
