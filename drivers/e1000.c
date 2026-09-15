#include "plexos.h"

/* Driver rete Intel generico: e1000 (82540EM/82545EM/82571 family).
   QEMU: -net nic,model=e1000 (8086:100E). Su HW reale: qualsiasi Intel class 02.
   Livello MAC reale: reset, MMIO, MAC/EEPROM, anelli RX/TX, link detect.
   Stack IP/DHCP: stub dichiarato (IP user-net stimato), TX/RX MAC operativi. */

#define E_CTRL     0x0000
#define E_STATUS   0x0008
#define E_EERD     0x0014
#define E_IMS      0x00D0
#define E_ICR      0x00C0
#define E_RCTL     0x0100
#define E_TCTL     0x0400
#define E_TIPG     0x0410
#define E_RDBAL    0x2800
#define E_RDBAH    0x2804
#define E_RDLEN    0x2808
#define E_RDH      0x2810
#define E_RDT      0x2818
#define E_TDBAL    0x3800
#define E_TDBAH    0x3804
#define E_TDLEN    0x3808
#define E_TDH      0x3810
#define E_TDT      0x3818
#define E_MTA      0x5200
#define E_RAL0     0x5400
#define E_RAH0     0x5404

#define CTRL_RST   (1<<26)
#define CTRL_ASDE  (1<<5)
#define CTRL_SLU   (1<<6)
#define STATUS_LU  (1<<1)
#define RCTL_EN    (1<<1)
#define RCTL_BAM   (1<<15)
#define RCTL_SECRC (1<<26)
#define TCTL_EN    (1<<1)
#define TCTL_PSP   (1<<3)
#define TX_CMD_EOP (1<<0)
#define TX_CMD_IFCS (1<<1)
#define TX_CMD_RS  (1<<3)
#define TX_STA_DD  (1<<0)
#define RX_STA_DD  (1<<0)

typedef struct { u32 addr_lo, addr_hi; u16 len, csum; u8 status, err; u16 special; } __attribute__((packed)) rx_desc_t;
typedef struct { u32 addr_lo, addr_hi; u16 len; u8 cso, cmd, status, css; u16 special; } __attribute__((packed)) tx_desc_t;

#define NRX 32
#define NTX 32
#define RBUFSZ 2048

static int present=0, enabled=1, link=0;
static pci_dev_t* dev=NULL;
static u32 mmio=0;
static u8 mac[6]={0};
static char name[56]="non trovata";
static char ip[56]="non assegnato";
static u32 tx_ok=0, rx_ok=0;
static int rx_cur=0, tx_cur=0;

static rx_desc_t rx_ring[NRX] __attribute__((aligned(16)));
static tx_desc_t tx_ring[NTX] __attribute__((aligned(16)));
static u8 rx_bufs[NRX][RBUFSZ] __attribute__((aligned(32)));
static u8 tx_bufs[NTX][RBUFSZ] __attribute__((aligned(32)));

static inline u32 R(u32 off){ return *(volatile u32*)(mmio+off); }
static inline void W(u32 off, u32 v){ *(volatile u32*)(mmio+off)=v; }

static void eeprom_mac(void){
    /* prova EEPROM: word 0,1,2 -> MAC */
    u8 m[6]={0};
    int ok=1;
    for(int i=0;i<3;i++){
        W(E_EERD, (1)|((u32)i<<8));
        int t=10000;
        while(t--){ if(R(E_EERD)&(1<<4)) break; }
        if(!(R(E_EERD)&(1<<4))){ ok=0; break; }
        u32 d=R(E_EERD)>>16;
        m[i*2]=(u8)(d&0xFF); m[i*2+1]=(u8)((d>>8)&0xFF);
    }
    if(ok && !(m[0]==0&&m[1]==0&&m[2]==0&&m[3]==0&&m[4]==0&&m[5]==0)
       && !(m[0]==0xFF&&m[1]==0xFF&&m[2]==0xFF&&m[3]==0xFF&&m[4]==0xFF&&m[5]==0xFF)){
        memcpy(mac,m,6);
    }
}

static void net_irq(regs_t* r){ UNUSED(r); if(mmio) (void)R(E_ICR); }

void net_init(void){
    present=0; link=0; enabled=1;
    strcpy(ip,"non assegnato");
    /* cerca Intel network qualsiasi (generico), preferisci 100E/100F/10D3 */
    pci_dev_t* f=pci_find(0x8086,0x100E);
    if(!f) f=pci_find(0x8086,0x100F);
    if(!f) f=pci_find(0x8086,0x10D3);
    if(!f) f=pci_find_class(0x02,0x00);
    if(!f){ kprintf("[NET] nessuna scheda Intel/e1000\n"); strcpy(name,"non trovata"); return; }
    if(f->vendor!=0x8086){ ksnprintf(name,sizeof(name),"NIC %04x:%04x (non Intel)", f->vendor,f->device); kprintf("[NET] %s: driver generico Intel non applicabile\n", name); return; }
    dev=f;
    pci_enable_mmio_busmaster(dev);
    mmio=pci_bar_addr(dev,0);
    if(!mmio||(mmio&1)){ kprintf("[NET] BAR0 non MMIO valida\n"); return; }
    ksnprintf(name,sizeof(name),"Intel e1000 %04x (MMIO %x)", dev->device, mmio);
    /* reset */
    W(E_IMS,0);
    (void)R(E_ICR);
    W(E_CTRL, R(E_CTRL)|CTRL_RST);
    sleep_ms(20);
    { int t=100000; while((R(E_CTRL)&CTRL_RST)&&t--); } /* mai hang su HW strano */
    sleep_ms(20);
    /* link up */
    W(E_CTRL, R(E_CTRL)|CTRL_ASDE|CTRL_SLU);
    /* MTA pulita */
    for(int i=0;i<128;i++) W(E_MTA+i*4,0);
    /* MAC da registri o EEPROM */
    {
        u32 ral=R(E_RAL0), rah=R(E_RAH0);
        if((rah&0x80000000)&&ral!=0&&ral!=0xFFFFFFFF){
            mac[0]=ral&0xFF; mac[1]=(ral>>8)&0xFF; mac[2]=(ral>>16)&0xFF; mac[3]=(ral>>24)&0xFF;
            mac[4]=rah&0xFF; mac[5]=(rah>>8)&0xFF;
        } else {
            eeprom_mac();
            if(!(mac[0]==0&&mac[1]==0&&mac[2]==0&&mac[3]==0&&mac[4]==0&&mac[5]==0)){
                u32 lo=mac[0]|(mac[1]<<8)|(mac[2]<<16)|(mac[3]<<24);
                u32 hi=mac[4]|(mac[5]<<8)|0x80000000;
                W(E_RAL0,lo); W(E_RAH0,hi);
            } else {
                /* QEMU senza EEPROM: MAC fissa di prova QEMU-like */
                mac[0]=0x52;mac[1]=0x54;mac[2]=0x00;mac[3]=0x12;mac[4]=0x34;mac[5]=0x56;
                W(E_RAL0,mac[0]|(mac[1]<<8)|(mac[2]<<16)|(mac[3]<<24));
                W(E_RAH0,mac[4]|(mac[5]<<8)|0x80000000);
            }
        }
    }
    /* RX ring */
    memset(rx_ring,0,sizeof(rx_ring));
    for(int i=0;i<NRX;i++){ rx_ring[i].addr_lo=(u32)rx_bufs[i]; rx_ring[i].addr_hi=0; rx_ring[i].status=0; }
    W(E_RDBAL,(u32)rx_ring); W(E_RDBAH,0);
    W(E_RDLEN,NRX*sizeof(rx_desc_t));
    W(E_RDH,0); W(E_RDT,NRX-1);
    W(E_RCTL, RCTL_EN|RCTL_BAM|RCTL_SECRC);
    /* TX ring */
    memset(tx_ring,0,sizeof(tx_ring));
    for(int i=0;i<NTX;i++){ tx_ring[i].status=TX_STA_DD; }
    W(E_TDBAL,(u32)tx_ring); W(E_TDBAH,0);
    W(E_TDLEN,NTX*sizeof(tx_desc_t));
    W(E_TDH,0); W(E_TDT,0);
    W(E_TCTL, TCTL_EN|TCTL_PSP|(0x10<<4)|(0x40<<12));
    W(E_TIPG, 10|(10<<10)|(10<<20));
    /* IRQ: solo clear-on-read, polling primario */
    if(dev->irq<16){ register_interrupt_handler(32+dev->irq, net_irq); }
    present=1;
    link=(R(E_STATUS)&STATUS_LU)?1:0;
    if(link) strcpy(ip,"10.0.2.15 via DHCP (user-net, stimato)");
    kprintf("[NET] %s MAC %02x:%02x:%02x:%02x:%02x:%02x link=%s IRQ=%d\n",
        name, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], link?"SU":"GIU", dev->irq);
}

int net_present(void){ return present; }
const char* net_name(void){ return name; }
u8* net_mac(void){ return mac; }
void net_mac_str(char* buf){ ksnprintf(buf,24,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]); }
int net_link(void){ if(present&&mmio) link=(R(E_STATUS)&STATUS_LU)?1:0; return link; }
int net_enabled(void){ return enabled; }
void net_set_enabled(int on){
    enabled=on?1:0;
    if(!present||!mmio) return;
    if(enabled){ W(E_RCTL,R(E_RCTL)|RCTL_EN); W(E_TCTL,R(E_TCTL)|TCTL_EN); }
    else { W(E_RCTL,R(E_RCTL)&~RCTL_EN); W(E_TCTL,R(E_TCTL)&~TCTL_EN); }
    kprintf("[NET] %s\n", enabled?"abilitata":"disabilitata");
}
u32 net_tx_count(void){ return tx_ok; }
u32 net_rx_count(void){ return rx_ok; }
const char* net_ip(void){ return ip; }

void net_poll(void){
    if(!present||!enabled||!mmio) return;
    link=(R(E_STATUS)&STATUS_LU)?1:0;
    /* drena RX */
    for(int n=0;n<NRX;n++){
        rx_desc_t* d=&rx_ring[rx_cur];
        if(!(d->status&RX_STA_DD)) break;
        rx_ok++;
        d->status=0;
        W(E_RDT,rx_cur);
        rx_cur=(rx_cur+1)%NRX;
    }
}

int net_tx_test(void){
    if(!present||!mmio||!enabled) return -1;
    /* frame broadcast 60B: dst FF*, src MAC, type 0x88B5 (test NexonTech) */
    u8* b=tx_bufs[tx_cur];
    memset(b,0,64);
    for(int i=0;i<6;i++) b[i]=0xFF;
    memcpy(b+6,mac,6);
    b[12]=0x88; b[13]=0xB5;
    const char* msg="PlexOS NexonTech net-test";
    memcpy(b+14,msg,strlen(msg));
    tx_desc_t* d=&tx_ring[tx_cur];
    d->addr_lo=(u32)b; d->addr_hi=0; d->len=60;
    d->cmd=TX_CMD_EOP|TX_CMD_IFCS|TX_CMD_RS; d->status=0;
    int cur=tx_cur; tx_cur=(tx_cur+1)%NTX;
    W(E_TDT,tx_cur);
    int t=10000;
    while(t--){ if(d->status&TX_STA_DD) break; }
    if(d->status&TX_STA_DD){ tx_ok++; return 0; }
    return -1;
}
