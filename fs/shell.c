#include "plexos.h"

/* Shell PlexOS: comandi di gestione sistema e file */

static void out_append(char* out, size_t outsz, const char* s){
    size_t a=strlen(out), b=strlen(s);
    if(a+b+1>=outsz) b=outsz-a-1;
    if((int)b<=0) return;
    memcpy(out+a,s,b); out[a+b]=0;
}

static int is_cmd(const char* line, const char* cmd){
    size_t l=strlen(cmd);
    return strncmp(line,cmd,l)==0 && (line[l]==0||line[l]==' '||line[l]=='\t');
}
static const char* skip(const char* s){
    while(*s==' '||*s=='\t') s++;
    return s;
}
static void first_arg(const char* line, char* buf, size_t n){
    /* estrae il primo argomento dopo il comando */
    const char* p=skip(line);
    while(*p&&*p!=' '&&*p!='\t') p++;
    p=skip(p);
    size_t i=0;
    int quote=0;
    if(*p=='"'||*p=='\''){ quote=*p; p++; }
    while(*p && i+1<n){
        if(quote){ if(*p==quote) break; buf[i++]=*p++; }
        else { if(*p==' '||*p=='\t') break; buf[i++]=*p++; }
    }
    buf[i]=0;
}
static void rest_args(const char* line, char* buf, size_t n){
    const char* p=skip(line);
    while(*p&&*p!=' '&&*p!='\t') p++;
    p=skip(p);
    /* salta primo arg se vogliamo il resto dal secondo? no: ritorna tutto dopo comando */
    strncpy(buf,p,n-1); buf[n-1]=0;
}

/* cronologia globale shell per `history` */
static char sh_hist[16][128];
static int sh_hn=0;
static void sh_push(const char* line){
    if(!line||!*line) return;
    if(sh_hn>0&&strcmp(sh_hist[sh_hn-1],line)==0) return;
    if(sh_hn<16){ strncpy(sh_hist[sh_hn],line,127); sh_hist[sh_hn][127]=0; sh_hn++; }
    else { for(int i=0;i<15;i++) strcpy(sh_hist[i],sh_hist[i+1]); strncpy(sh_hist[15],line,127); sh_hist[15][127]=0; }
}
static u32 sh_rand=0x12345678;
static u32 sh_rnd(u32 m){ sh_rand=sh_rand*1103515245+12345; return m?((sh_rand>>16)%m):0; }

/* somma ricorsiva con cap profondita */
static u32 du_sum(fs_node_t* n, int depth){
    if(!n||depth>16) return 0;
    if(!n->is_dir) return n->size;
    u32 t=0;
    for(fs_node_t* c=n->child;c;c=c->next) t+=du_sum(c,depth+1);
    return t;
}
static void tree_rec(fs_node_t* n, char* out, size_t outsz, int depth, const char* pre){
    if(!n||depth>8) return;
    int i=0;
    for(fs_node_t* c=n->child;c&&i<40;c=c->next,i++){
        char b[160];
        ksnprintf(b,sizeof(b),"%s%s %s%s\n", pre, (c->next?"|--":"`--"), c->name, c->is_dir?"/":"");
        out_append(out,outsz,b);
        if(c->is_dir){
            char np[32]; ksnprintf(np,sizeof(np),"%s%s   ", pre, (c->next?"|":" "));
            tree_rec(c,out,outsz,depth+1,np);
        }
    }
}
static void find_rec(fs_node_t* n, const char* pat, char* out, size_t outsz, int depth, const char* base){
    if(!n||depth>12) return;
    for(fs_node_t* c=n->child;c;c=c->next){
        char full[256]; ksnprintf(full,sizeof(full),"%s%s%s", base, strcmp(base,"/")==0?"":"/", c->name);
        if(strstr(c->name,pat)){
            char b[300]; ksnprintf(b,sizeof(b),"%s%s\n", full, c->is_dir?"/":"");
            out_append(out,outsz,b);
        }
        if(c->is_dir) find_rec(c,pat,out,outsz,depth+1,full);
    }
}

void shell_execute(const char* line, char* out, size_t outsz, int* clear_req, int* exit_req){
    out[0]=0;
    if(clear_req) *clear_req=0;
    if(exit_req) *exit_req=0;
    sh_push(skip(line));
    char l[512]; strncpy(l,line,511); l[511]=0;
    /* trim */
    const char* s=skip(l);
    if(!*s) return;
    /* copia trimmata in l */
    {
        char t[512]; strncpy(t,s,511); t[511]=0;
        int e=strlen(t); while(e>0&&(t[e-1]==' '||t[e-1]=='\t')){t[e-1]=0;e--;}
        strcpy(l,t); s=l;
    }

    if(is_cmd(s,"help")||is_cmd(s,"aiuto")){
        char ha[64]; first_arg(s,ha,sizeof(ha));
        if(ha[0]&&strcmp(ha,"help")!=0){
            char hb[192];
            if(strcmp(ha,"calc")==0) strcpy(hb,"calc <expr>: + - * / % ^ ( )  es: calc 2*(3+4)^2\n");
            else if(strcmp(ha,"write")==0) strcpy(hb,"write <file> <testo>: sovrascrive il file\n");
            else if(strcmp(ha,"open")==0) strcpy(hb,"open <app>: term files calc browser edit settings\n");
            else if(strcmp(ha,"grep")==0) strcpy(hb,"grep <pattern> <file>: righe contenenti pattern\n");
            else if(strcmp(ha,"find")==0) strcpy(hb,"find [path] <nome>: cerca per nome (sottostringa)\n");
            else if(strcmp(ha,"head")==0||strcmp(ha,"tail")==0) strcpy(hb,"head/tail [-N] <file>: prime/ultime N righe (def 10)\n");
            else if(strcmp(ha,"theme")==0) strcpy(hb,"theme 0=scuro 1=chiaro 2=blu\n");
            else if(strcmp(ha,"sleep")==0) strcpy(hb,"sleep <s>: pausa 0-10s (blocca la GUI, usala poco)\n");
            else ksnprintf(hb,sizeof(hb),"help %s: vedi guida generale (help).\n", ha);
            out_append(out,outsz,hb); return;
        }
        out_append(out,outsz,
            "Comandi PlexOS (1/2 - file):\n"
            "  ls dir cd pwd mkdir touch cat echo write rm cp mv\n"
            "  tree [p] find [p] <n> grep <pat> <f> head/tail [-n] <f>\n"
            "  wc <f> du [p] df free stat <f> mount open <app>\n"
            "Comandi (2/2 - sistema):\n"
            "  sysinfo meminfo cpuinfo gpu pci netinfo netsend netup\n"
            "  netdown netstat vinfo uname dmesg ps uptime date cal\n"
            "  calc theme apps plexfetch history sleep <s> fortune\n"
            "  clear reboot shutdown exit | dettagli: help <cmd>\n");
        return;
    }
    if(is_cmd(s,"clear")||is_cmd(s,"cls")){ if(clear_req)*clear_req=1; return; }
    if(is_cmd(s,"exit")||is_cmd(s,"quit")){ if(exit_req)*exit_req=1; out_append(out,outsz,"Chiusura terminale...\n"); return; }
    if(is_cmd(s,"echo")){
        char r[480]; rest_args(s,r,sizeof(r));
        out_append(out,outsz,r); out_append(out,outsz,"\n"); return;
    }
    if(is_cmd(s,"pwd")){
        char b[256]; fs_cwd_str(b,sizeof(b));
        out_append(out,outsz,b); out_append(out,outsz,"\n"); return;
    }
    if(is_cmd(s,"ls")||is_cmd(s,"dir")){
        char a[256]; first_arg(s,a,sizeof(a));
        fs_node_t* d = a[0]?fs_resolve(a):fs_cwd();
        if(!d){ out_append(out,outsz,"ls: percorso non trovato\n"); return; }
        if(!d->is_dir){
            char b[128]; ksnprintf(b,sizeof(b),"%s  (%d bytes)\n", d->name, d->size);
            out_append(out,outsz,b); return;
        }
        if(!d->child){ out_append(out,outsz,"(vuota)\n"); return; }
        for(fs_node_t* c=d->child;c;c=c->next){
            char b[160];
            if(c->is_dir) ksnprintf(b,sizeof(b),"[DIR]  %s/\n", c->name);
            else ksnprintf(b,sizeof(b),"[FILE] %s  (%d bytes)\n", c->name, c->size);
            out_append(out,outsz,b);
        }
        return;
    }
    if(is_cmd(s,"cd")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ fs_set_cwd("/"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n||!n->is_dir){ out_append(out,outsz,"cd: cartella non trovata\n"); return; }
        /* aggiorna cwd: trova path assoluto */
        /* trucco: sposta cwd direttamente */
        /* fs_set_cwd fa resolve da cwd, qui calcoliamo */
        extern void fs_set_cwd_ptr(fs_node_t* n);
        /* fallback: usa fs_set_cwd se assoluto altrimenti naviga */
        /* implementazione semplice: */
        {
            /* rendi assoluto */
            char cur[256]; fs_cwd_str(cur,sizeof(cur));
            char abs[256];
            if(a[0]=='/') strcpy(abs,a);
            else { strcpy(abs,cur); if(strcmp(cur,"/")!=0) strcat(abs,"/"); strcat(abs,a); }
            fs_set_cwd(abs);
        }
        return;
    }
    if(is_cmd(s,"mkdir")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: mkdir <path>\n"); return; }
        if(fs_mkdir(a)==0) out_append(out,outsz,"Cartella creata.\n");
        else out_append(out,outsz,"mkdir: impossibile creare (esiste o path errato)\n");
        return;
    }
    if(is_cmd(s,"touch")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: touch <file>\n"); return; }
        if(fs_touch(a)==0) out_append(out,outsz,"File creato.\n");
        else out_append(out,outsz,"touch: errore\n");
        return;
    }
    if(is_cmd(s,"cat")||is_cmd(s,"type")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: cat <file>\n"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n){ out_append(out,outsz,"cat: file non trovato\n"); return; }
        if(n->is_dir){ out_append(out,outsz,"cat: e' una cartella\n"); return; }
        if(!n->data||n->size==0){ out_append(out,outsz,"(file vuoto)\n"); return; }
        /* copia a blocchi */
        u32 off=0;
        while(off<n->size){
            char chunk[256]; u32 c=n->size-off; if(c>200)c=200;
            memcpy(chunk,n->data+off,c); chunk[c]=0;
            out_append(out,outsz,chunk); off+=c;
        }
        if(n->size&&n->data[n->size-1]!='\n') out_append(out,outsz,"\n");
        return;
    }
    if(is_cmd(s,"write")){
        /* write <file> <testo...> */
        const char* p=skip(s)+5; p=skip(p);
        char f[128]; int i=0;
        while(*p&&*p!=' '&&*p!='\t'&&i<127){f[i++]=*p++;} f[i]=0;
        p=skip(p);
        if(!f[0]){ out_append(out,outsz,"uso: write <file> <testo>\n"); return; }
        if(fs_write_file(f,p,strlen(p))==0) out_append(out,outsz,"Scritto.\n");
        else out_append(out,outsz,"write: errore\n");
        return;
    }
    if(is_cmd(s,"rm")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: rm <path>\n"); return; }
        int r=fs_rm(a);
        if(r==0) out_append(out,outsz,"Eliminato.\n");
        else if(r==-2) out_append(out,outsz,"rm: cartella non vuota\n");
        else out_append(out,outsz,"rm: non trovato\n");
        return;
    }
    if(is_cmd(s,"cp")){
        /* cp src dst */
        const char* p=skip(s)+2; p=skip(p);
        char a[128],b[128]; int i=0;
        while(*p&&*p!=' '&&*p!='\t'&&i<127){a[i++]=*p++;} a[i]=0; p=skip(p);
        i=0; while(*p&&*p!=' '&&*p!='\t'&&i<127){b[i++]=*p++;} b[i]=0;
        if(!a[0]||!b[0]){ out_append(out,outsz,"uso: cp <src> <dst>\n"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n||n->is_dir){ out_append(out,outsz,"cp: solo file (no dir)\n"); return; }
        fs_touch(b);
        if(fs_write_file(b,n->data?n->data:"",n->size)==0) out_append(out,outsz,"Copiato.\n");
        else out_append(out,outsz,"cp: errore\n");
        return;
    }
    if(is_cmd(s,"mv")){
        const char* p=skip(s)+2; p=skip(p);
        char a[128],b[128]; int i=0;
        while(*p&&*p!=' '&&*p!='\t'&&i<127){a[i++]=*p++;} a[i]=0; p=skip(p);
        i=0; while(*p&&*p!=' '&&*p!='\t'&&i<127){b[i++]=*p++;} b[i]=0;
        if(!a[0]||!b[0]){ out_append(out,outsz,"uso: mv <src> <dst>\n"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n){ out_append(out,outsz,"mv: sorgente non trovata\n"); return; }
        /* crea dst copiando poi rm (solo file; per dir ricollega) */
        if(n->is_dir){
            if(fs_mkdir(b)!=0){ out_append(out,outsz,"mv: impossibile creare dst\n"); return; }
            /* sposta figli? semplificato: non supportato per dir non vuote */
            if(n->child){ out_append(out,outsz,"mv: dir non vuota non supportata, uso cp file singoli\n"); return; }
            fs_rm(a); out_append(out,outsz,"Spostato.\n"); return;
        }
        fs_touch(b);
        fs_write_file(b,n->data?n->data:"",n->size);
        fs_rm(a);
        out_append(out,outsz,"Spostato.\n");
        return;
    }
    if(is_cmd(s,"sysinfo")){
        char b[768];
        rtc_time_t t; rtc_read(&t); char ds[32]; rtc_format(ds,&t);
        char m[24]; net_mac_str(m);
        ksnprintf(b,sizeof(b),
            "PlexOS 1.0.0 -- sysinfo (NexonTech)\n  CPU: %s\n  %s | %d core | %d MHz\n"
            "  GPU: %s\n  RAM totale: %d KB  liberi: %d KB\n"
            "  Heap usata: %d bytes\n  Video: %dx%dx%d %s%s 60fps\n  HDD ATA: %s (%d settori)\n"
            "  NET: %s MAC %s link %s IP %s\n  PCI devices: %d\n  Uptime: %d s\n  Ora: %s\n  FS nodi: %d\n",
            cpu_short(), cpu_brand(), cpu_cores(), cpu_mhz(),
            gpu_name(),
            pmm_total_kb(), pmm_free_kb(), kmalloc_used(),
            fb_width(), fb_height(), fb_bpp(), fb_ready()?"framebuffer":"VGA-text",
            fb_double_buffered()?" dbl-buf":"",
            ata_present()?"presente":"assente", ata_sectors(),
            net_name(), m, net_link()?"SU":"GIU", net_ip(),
            pci_count(), uptime_sec(), ds, fs_count());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"meminfo")){
        char b[256];
        ksnprintf(b,sizeof(b),"Memoria:\n  totale: %d KB\n  libera: %d KB\n  heap: %d bytes\n",
            pmm_total_kb(), pmm_free_kb(), kmalloc_used());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"cpuinfo")){
        char b[320], cc[64];
        cpu_cache_str(cc,sizeof(cc));
        ksnprintf(b,sizeof(b),"CPU: %s\n  vendor=%s fam=%d mod=%x step=%d cores=%d\n  %d MHz misurati base=%d max=%d | %s\n  cache: %s | i3-1115G4: %s\n",
            cpu_brand(), cpu_vendor(), cpu_family(), cpu_model(), cpu_stepping(),
            cpu_cores(), cpu_mhz(), cpu_base_mhz(), cpu_max_mhz(), cpu_feats(), cc,
            cpu_is_i3_1115G4()?"RILEVATA":"profilo pronto (HW reale)");
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"gpu")){
        char b[224];
        ksnprintf(b,sizeof(b),"GPU: %s\n  %s  Intel: %s  UHD G4: %s\n  Video %dx%dx%d 60fps\n",
            gpu_name(), gpu_mem(), gpu_is_intel()?"si":"no", gpu_is_uhd_g4()?"si":"no",
            fb_width(), fb_height(), fb_bpp());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"netinfo")||is_cmd(s,"net")||is_cmd(s,"ifconfig")){
        char b[256], m[24]; net_mac_str(m);
        ksnprintf(b,sizeof(b),"Rete Intel generica:\n  scheda: %s\n  MAC: %s\n  link: %s  on: %s\n  IP: %s\n  TX: %d  RX: %d\n",
            net_name(), m, net_link()?"SU":"GIU", net_enabled()?"si":"no",
            net_ip(), net_tx_count(), net_rx_count());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"netsend")){
        int r=net_tx_test();
        out_append(out,outsz,r==0?"TX test: frame inviato.\n":"TX test fallito (no scheda/link?).\n"); return;
    }
    if(is_cmd(s,"netup")){ net_set_enabled(1); out_append(out,outsz,"Rete abilitata.\n"); return; }
    if(is_cmd(s,"netdown")){ net_set_enabled(0); out_append(out,outsz,"Rete disabilitata.\n"); return; }
    if(is_cmd(s,"pci")){
        char b[128];
        ksnprintf(b,sizeof(b),"Dispositivi PCI: %d\n", pci_count());
        out_append(out,outsz,b);
        for(int i=0;i<pci_count();i++){
            pci_dev_t* d=pci_get(i);
            ksnprintf(b,sizeof(b),"  %02x:%02x.%d %04x:%04x %s\n", d->bus,d->slot,d->func,d->vendor,d->device,pci_class_name(d->class_));
            out_append(out,outsz,b);
        }
        return;
    }
    if(is_cmd(s,"ps")){
        out_append(out,outsz,"PID  NOME\n  1   kernel/plexos\n  2   gui/desktop-60fps\n  3   PlexTerm\n  4   PlexFiles\n  5   PlexCalc\n  6   PlexBrowse\n  7   PlexEdit\n  8   Impostazioni\n  9   net/e1000-poll\n"); return;
    }
    if(is_cmd(s,"uptime")){
        char b[64]; ksnprintf(b,sizeof(b),"up %d secondi (%d ticks)\n", uptime_sec(), timer_ticks());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"date")){
        rtc_time_t t; rtc_read(&t); char ds[32]; rtc_format(ds,&t);
        out_append(out,outsz,ds); out_append(out,outsz,"\n"); return;
    }
    if(is_cmd(s,"uname")||is_cmd(s,"arch")){
        if(is_cmd(s,"arch")){ out_append(out,outsz,"i386\n"); return; }
        out_append(out,outsz,"PlexOS plexos 1.0.0 i386 PlexDE/60fps NexonTech\n"); return;
    }
    if(is_cmd(s,"free")){
        char b[160];
        ksnprintf(b,sizeof(b),"RAM: %d KB tot  %d KB liberi  heap %d B  nodi FS %d\n",
            pmm_total_kb(), pmm_free_kb(), kmalloc_used(), fs_count());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"df")){
        char b[192];
        ksnprintf(b,sizeof(b),"FS   Nodi      Usato\n/    vfs-ram   %d nodi, heap %d B\n/dev/hda ATA %s %d settori (%d MB)\n",
            fs_count(), kmalloc_used(), ata_present()?"presente":"assente",
            ata_sectors(), ata_sectors()/2048);
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"du")){
        char a[256]; first_arg(s,a,sizeof(a));
        fs_node_t* n=a[0]?fs_resolve(a):fs_cwd();
        if(!n){ out_append(out,outsz,"du: percorso non trovato\n"); return; }
        char b[64]; ksnprintf(b,sizeof(b),"%d byte in %s\n", du_sum(n,0), a[0]?a:".");
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"stat")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: stat <file>\n"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n){ out_append(out,outsz,"stat: non trovato\n"); return; }
        char b[192];
        if(n->is_dir){ int k=0; for(fs_node_t* c=n->child;c;c=c->next) k++; ksnprintf(b,sizeof(b),"%s: cartella, %d voci\n", a, k); }
        else ksnprintf(b,sizeof(b),"%s: file, %d byte\n", a, n->size);
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"tree")){
        char a[256]; first_arg(s,a,sizeof(a));
        fs_node_t* n=a[0]?fs_resolve(a):fs_cwd();
        if(!n||!n->is_dir){ out_append(out,outsz,"tree: cartella non valida\n"); return; }
        out_append(out,outsz,a[0]?a:"con."); out_append(out,outsz,"\n");
        tree_rec(n,out,outsz,0,"");
        return;
    }
    if(is_cmd(s,"find")){
        char r[256]; rest_args(s,r,sizeof(r));
        /* split: [path] nome (ultima parola = pattern) */
        char path[256]="", pat[128]="";
        int len=strlen(r);
        while(len>0&&(r[len-1]==' '||r[len-1]=='\t')){r[len-1]=0;len--;}
        char* sp=NULL;
        for(int i=len-1;i>=0;i--) if(r[i]==' '||r[i]=='\t'){sp=r+i;break;}
        if(sp){ *sp=0; strncpy(pat,skip(sp+1),127); strncpy(path,skip(r),255); }
        else strncpy(pat,r,127);
        if(!pat[0]){ out_append(out,outsz,"uso: find [path] <nome>\n"); return; }
        fs_node_t* n=path[0]?fs_resolve(path):fs_cwd();
        if(!n||!n->is_dir){ out_append(out,outsz,"find: path non valida\n"); return; }
        find_rec(n,pat,out,outsz,0,path[0]?path:"/");
        return;
    }
    if(is_cmd(s,"grep")){
        const char* p=skip(s)+4; p=skip(p);
        char pat[96]; int i=0;
        while(*p&&*p!=' '&&*p!='\t'&&i<95){pat[i++]=*p++;} pat[i]=0; p=skip(p);
        char fn[128]; i=0;
        while(*p&&*p!=' '&&*p!='\t'&&i<127){fn[i++]=*p++;} fn[i]=0;
        if(!pat[0]||!fn[0]){ out_append(out,outsz,"uso: grep <pattern> <file>\n"); return; }
        fs_node_t* n=fs_resolve(fn);
        if(!n||n->is_dir||!n->data){ out_append(out,outsz,"grep: file non valido/vuoto\n"); return; }
        int row=1, hits=0;
        char* q=n->data; char* e=n->data+n->size;
        while(q<e&&hits<60){
            char* nl=q; while(nl<e&&*nl!='\n') nl++;
            int ll=nl-q; if(ll>140) ll=140;
            char line[150]; memcpy(line,q,ll); line[ll]=0;
            /* match? */
            const char* h=line; int ok=0;
            while(*h){ if(strncmp(h,pat,strlen(pat))==0){ok=1;break;} h++; }
            if(ok){ char b[180]; ksnprintf(b,sizeof(b),"%d:%s\n", row, line); out_append(out,outsz,b); hits++; }
            row++; q=(*nl=='\n')?nl+1:nl;
            if(nl>=e) break;
        }
        if(!hits) out_append(out,outsz,"(nessuna riga)\n");
        return;
    }
    if(is_cmd(s,"head")||is_cmd(s,"tail")){
        int istail=is_cmd(s,"tail");
        const char* p=skip(s)+4; p=skip(p);
        int n=10; char fn[128]="";
        if(*p=='-'){ p++; n=atoi(p); while(*p&&*p!=' '&&*p!='\t') p++; p=skip(p); if(n<1)n=1; if(n>50)n=50; }
        { int i=0; while(*p&&*p!=' '&&*p!='\t'&&i<127){fn[i++]=*p++;} fn[i]=0; }
        if(!fn[0]){ out_append(out,outsz,"uso: head/tail [-N] <file>\n"); return; }
        fs_node_t* nd=fs_resolve(fn);
        if(!nd||nd->is_dir||!nd->data){ out_append(out,outsz,"file non valido/vuoto\n"); return; }
        /* conta righe */
        int rows=1; for(u32 k=0;k<nd->size;k++) if(nd->data[k]=='\n') rows++;
        int from=istail?(rows-n):0, to=istail?rows:n;
        if(from<0)from=0;
        int r=0; char* q=nd->data; char* ee=nd->data+nd->size;
        while(q<ee){
            char* nl=q; while(nl<ee&&*nl!='\n') nl++;
            if(r>=from&&r<to){ int ll=nl-q; if(ll>180)ll=180; char lb[190]; memcpy(lb,q,ll); lb[ll]=0; out_append(out,outsz,lb); out_append(out,outsz,"\n"); }
            r++; q=(*nl=='\n')?nl+1:nl;
            if(nl>=ee) break;
        }
        return;
    }
    if(is_cmd(s,"wc")){
        char a[256]; first_arg(s,a,sizeof(a));
        if(!a[0]){ out_append(out,outsz,"uso: wc <file>\n"); return; }
        fs_node_t* n=fs_resolve(a);
        if(!n||n->is_dir||!n->data){ out_append(out,outsz,"wc: file non valido\n"); return; }
        int rows=0,words=0,inw=0;
        for(u32 k=0;k<n->size;k++){ char ch=n->data[k]; if(ch=='\n')rows++; if(ch==' '||ch=='\t'||ch=='\n'){inw=0;} else if(!inw){inw=1;words++;} }
        char b[64]; ksnprintf(b,sizeof(b),"%d righe %d parole %d byte\n", rows, words, n->size);
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"mount")){
        char b[160];
        ksnprintf(b,sizeof(b),"/ su vfs-ram (%d nodi)\n/dev/hda: ATA %s, %d settori\n",
            fs_count(), ata_present()?"disco presente":"assente", ata_sectors());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"vinfo")){
        char b[192];
        ksnprintf(b,sizeof(b),"Video %dx%dx%d %s dbl-buf %s vsync 60fps PAT-WC %s\nGPU: %s\n",
            fb_width(), fb_height(), fb_bpp(), fb_ready()?"framebuffer":"VGA-text",
            fb_double_buffered()?"ON":"OFF", cpu_has_pat_wc()?"ON":"n/d", gpu_name());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"netstat")){
        char b[160], m[24]; net_mac_str(m);
        ksnprintf(b,sizeof(b),"%s\nMAC %s link %s IP %s TX %d RX %d\n",
            net_name(), m, net_link()?"SU":"GIU", net_ip(), net_tx_count(), net_rx_count());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"dmesg")){
        char tmp[1900]; dmesg_dump(tmp,sizeof(tmp));
        /* mostra solo la coda (out e' 2KB) */
        out_append(out,outsz,tmp); return;
    }
    if(is_cmd(s,"history")){
        if(!sh_hn){ out_append(out,outsz,"(vuota)\n"); return; }
        for(int k=0;k<sh_hn;k++){ char b[150]; ksnprintf(b,sizeof(b),"%d  %s\n", k+1, sh_hist[k]); out_append(out,outsz,b); }
        return;
    }
    if(is_cmd(s,"sleep")){
        char a[16]; first_arg(s,a,sizeof(a));
        int sec=atoi(a); if(sec<0)sec=0; if(sec>10)sec=10;
        char b[48]; ksnprintf(b,sizeof(b),"pausa %d s...\n", sec);
        out_append(out,outsz,b);
        if(sec>0) sleep_ms(sec*1000);
        return;
    }
    if(is_cmd(s,"cal")){
        char r[64]; rest_args(s,r,sizeof(r));
        rtc_time_t t; rtc_read(&t);
        int M=t.mon, Y=t.year;
        /* parse [mese anno] */
        { int v1=0,v2=0,n=0; const char* q=r;
          while(*q==' '||*q=='\t')q++; while(*q>='0'&&*q<='9'){v1=v1*10+(*q-'0');q++;n++;}
          while(*q==' '||*q=='\t')q++; while(*q>='0'&&*q<='9'){v2=v2*10+(*q-'0');q++;}
          if(n>0){ if(v2>0){M=v1;Y=v2;} else if(v1>31){Y=v1;} else {M=v1;} }
          if(M<1)M=1; if(M>12)M=12; if(Y<1970)Y=1970; if(Y>2100)Y=2100; }
        int dm[12]={31,28,31,30,31,30,31,31,30,31,30,31};
        if((Y%4==0&&Y%100!=0)||Y%400==0) dm[1]=29;
        /* Zeller: giorno settimana del 1 */
        int q_=1,mm=M,yy=Y; if(mm<3){mm+=12;yy--;}
        int h=(q_+ (13*(mm+1))/5 + yy + yy/4 - yy/100 + yy/400)%7; /* 0=sab */
        int wd=(h+6)%7; /* 0=lun */
        const char* mn[12]={"Gen","Feb","Mar","Apr","Mag","Giu","Lug","Ago","Set","Ott","Nov","Dic"};
        char b[64]; ksnprintf(b,sizeof(b),"   %s %d\nLu Ma Me Gi Ve Sa Do\n", mn[M-1], Y);
        out_append(out,outsz,b);
        char row[64]; int col=0; row[0]=0;
        for(int k=0;k<wd;k++){ strcat(row,"   "); col++; }
        for(int d_=1;d_<=dm[M-1];d_++){
            char c2[8]; ksnprintf(c2,sizeof(c2),"%2d ", d_);
            strcat(row,c2); col++;
            if(col==7){ strcat(row,"\n"); out_append(out,outsz,row); row[0]=0; col=0; }
        }
        if(row[0]){ strcat(row,"\n"); out_append(out,outsz,row); }
        return;
    }
    if(is_cmd(s,"fortune")){
        const char* q[]={
            "PlexOS: piccolo kernel, grandi idee. - NexonTech",
            "720p oggi, 1080p domani.",
            "Il miglior driver e' quello che non crasha.",
            "Leggi i log: dmesg e' tuo amico.",
            "i3-1115G4 + UHD G4: Tiger Lake ruggisce.",
            "Prova: calc 2*(3+4)^2 | open browser | theme 2",
            "60fps: un frame alla volta.",
            "F10 apre Plex Start. Mouse + tastiera = potere."
        };
        sh_rand+=timer_ticks()+uptime_sec();
        char b[128]; ksnprintf(b,sizeof(b),"%s\n", q[sh_rnd(8)]);
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"open")||is_cmd(s,"run")){
        char a[32]; first_arg(s,a,sizeof(a));
        for(char* p=a;*p;p++) *p=tolower_c(*p);
        int k=-1; const char* nm="";
        if(!strcmp(a,"term")||!strcmp(a,"terminal")||!strcmp(a,"terminale")){k=0;nm="Terminale";}
        else if(!strcmp(a,"files")||!strcmp(a,"file")||!strcmp(a,"manager")){k=1;nm="File Manager";}
        else if(!strcmp(a,"calc")||!strcmp(a,"calcolatrice")){k=2;nm="Calcolatrice";}
        else if(!strcmp(a,"browser")||!strcmp(a,"web")){k=3;nm="Browser";}
        else if(!strcmp(a,"edit")||!strcmp(a,"editor")){k=4;nm="Editor";}
        else if(!strcmp(a,"sys")||!strcmp(a,"sysmon")||!strcmp(a,"settings")||!strcmp(a,"impostazioni")){k=5;nm="Impostazioni";}
        if(k<0){ out_append(out,outsz,"uso: open term|files|calc|browser|edit|settings\n"); return; }
        if(gui_open(k,nm)){ char b[64]; ksnprintf(b,sizeof(b),"Aperta: %s (vedi desktop)\n", nm); out_append(out,outsz,b); }
        else out_append(out,outsz,"troppe finestre aperte.\n");
        return;
    }
    if(is_cmd(s,"kill")){
        out_append(out,outsz,"GUI cooperativa: chiudi con [X] o taskbar.\nPID kernel non killabili.\n"); return;
    }
    if(is_cmd(s,"lsusb")){
        out_append(out,outsz,"USB: nessun driver (attivi: PS/2, ATA, e1000).\nTastiere USB su HW reale: via BIOS-legacy se attivo.\n"); return;
    }
    if(is_cmd(s,"lscpu")){
        char b[256];
        ksnprintf(b,sizeof(b),"%s\nfam=%d mod=%x %d core %d MHz | %s\n",
            cpu_brand(), cpu_family(), cpu_model(), cpu_cores(), cpu_mhz(), cpu_feats());
        out_append(out,outsz,b); return;
    }
    if(is_cmd(s,"calc")){
        char e[256]; rest_args(s,e,sizeof(e));
        if(!e[0]){ out_append(out,outsz,"uso: calc <espressione>\n"); return; }
        int ok=0; double v=calc_eval(e,&ok);
        char b[64];
        if(!ok){ out_append(out,outsz,"calc: espressione non valida\n"); return; }
        ftoa_simple(v,b,4);
        out_append(out,outsz,b); out_append(out,outsz,"\n"); return;
    }
    if(is_cmd(s,"theme")){
        char a[16]; first_arg(s,a,sizeof(a));
        if(!a[0]){ char b[64]; ksnprintf(b,sizeof(b),"tema attuale: %d (0=scuro 1=chiaro 2=blu)\n", gui_theme()); out_append(out,outsz,b); return; }
        int t=atoi(a); if(t<0)t=0; if(t>2)t=2;
        gui_set_theme(t);
        out_append(out,outsz,"Tema cambiato.\n"); return;
    }
    if(is_cmd(s,"apps")){
        out_append(out,outsz,"App PlexOS (6):\n 1 Terminale   2 File Manager\n 3 Calcolatrice 4 Browser\n 5 Editor       6 Impostazioni (sistema/rete/temi)\nUsa il mouse o il menu Start.\n"); return;
    }
    if(is_cmd(s,"plexfetch")||is_cmd(s,"neofetch")||is_cmd(s,"about")){
        char b[1024], m[24];
        rtc_time_t t; rtc_read(&t); char ds[32]; rtc_format(ds,&t);
        net_mac_str(m);
        ksnprintf(b,sizeof(b),
            "  ____  _           ___  ____\n"
            " |  _ \\| | _____  / _ \\/ ___|\n"
            " | |_) | |/ _ \\ \\/ / \\ \\___ \\\n"
            " |  __/| |  __/>  < _  |___) |\n"
            " |_|   |_|\\___/_/\\_(_)/____/  by NexonTech\n"
            "-------------------------------\n"
            " OS: PlexOS 1.0.0  Kernel: 32-bit Multiboot\n"
            " Host: plexos  Uptime: %d s  Shell: PlexTerm\n"
            " Risoluzione: %dx%dx%d 60fps  DE: PlexDE (dbl-buf)\n"
            " CPU: %s\n"
            " GPU: %s\n"
            " Memoria: %d KB tot / %d KB liberi (heap %d B)\n"
            " Disco: ATA %s (%d settori)  PCI: %d dev\n"
            " Rete: %s %s IP %s\n"
            " Data: %s  FS nodi: %d  Tema: %d\n",
            uptime_sec(), fb_width(), fb_height(), fb_bpp(),
            cpu_brand(), gpu_name(),
            pmm_total_kb(), pmm_free_kb(), kmalloc_used(),
            ata_present()?"OK":"n/d", ata_sectors(), pci_count(),
            net_name(), m, net_ip(), ds, fs_count(), gui_theme());
        out_append(out,outsz,b);
        return;
    }
    if(is_cmd(s,"whoami")){ out_append(out,outsz,"user@plexos\n"); return; }
    if(is_cmd(s,"hostname")){ out_append(out,outsz,"plexos\n"); return; }
    if(is_cmd(s,"ver")||is_cmd(s,"version")){
        fs_node_t* n=fs_resolve("/sys/version");
        if(n&&n->data) { out_append(out,outsz,n->data); out_append(out,outsz,"\n"); }
        else out_append(out,outsz,"PlexOS 1.0.0\n");
        return;
    }
    if(is_cmd(s,"reboot")){
        out_append(out,outsz,"Riavvio...\n");
        sys_reboot();
        for(;;){ cli(); hlt(); }
    }
    if(is_cmd(s,"shutdown")||is_cmd(s,"poweroff")||is_cmd(s,"halt")){
        out_append(out,outsz,"Spegnimento...\n");
        sys_poweroff();
        for(;;){ cli(); hlt(); }
    }
    /* comando sconosciuto */
    {
        char b[300];
        /* prendi prima parola */
        char w[64]; int i=0; const char* p=s;
        while(*p&&*p!=' '&&*p!='\t'&&i<63){w[i++]=*p++;} w[i]=0;
        ksnprintf(b,sizeof(b),"comando non trovato: %s\nScrivi 'help'.\n", w);
        out_append(out,outsz,b);
    }
}
