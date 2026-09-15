#include "plexos.h"

struct idt_entry { u16 base_lo; u16 sel; u8 always0; u8 flags; u16 base_hi; } __attribute__((packed));
struct idt_ptr { u16 limit; u32 base; } __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr ip;
extern void idt_flush(u32);

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static isr_t handlers[256];

static void idt_set(int n, u32 base, u16 sel, u8 flags){
    idt[n].base_lo=base&0xFFFF; idt[n].base_hi=(base>>16)&0xFFFF;
    idt[n].sel=sel; idt[n].always0=0; idt[n].flags=flags;
}

void register_interrupt_handler(u8 n, isr_t h){ handlers[n]=h; }

static const char* exc_msg[] = {
    "Division By Zero","Debug","NMI","Breakpoint","Overflow","Bound Range","Invalid Opcode",
    "Device Not Available","Double Fault","Coprocessor Overrun","Invalid TSS","Segment Not Present",
    "Stack Fault","General Protection","Page Fault","Reserved","x87 FPU","Alignment Check",
    "Machine Check","SIMD Exception","Virtualization","Reserved","Reserved","Reserved","Reserved",
    "Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved"
};

void isr_handler(regs_t* r){
    if(r->int_no < 32){
        kprintf("[EXC %d] %s err=%x eip=%x\n", r->int_no, exc_msg[r->int_no], r->err_code, r->eip);
        if(r->int_no==14){
            u32 cr2; __asm__ volatile("mov %%cr2,%0":"=r"(cr2));
            kprintf("  pagefault addr=%x\n", cr2);
        }
        if(handlers[r->int_no]) handlers[r->int_no](r);
        else panic("Eccezione CPU non gestita");
    } else if(handlers[r->int_no]) handlers[r->int_no](r);
}

void irq_handler(regs_t* r){
    if(r->int_no>=40) outb(0xA0,0x20);
    outb(0x20,0x20);
    if(handlers[r->int_no]) handlers[r->int_no](r);
}

void idt_init(void){
    for(int i=0;i<256;i++) handlers[i]=0;
    idt_set(0,(u32)isr0,0x08,0x8E);  idt_set(1,(u32)isr1,0x08,0x8E);
    idt_set(2,(u32)isr2,0x08,0x8E);  idt_set(3,(u32)isr3,0x08,0x8E);
    idt_set(4,(u32)isr4,0x08,0x8E);  idt_set(5,(u32)isr5,0x08,0x8E);
    idt_set(6,(u32)isr6,0x08,0x8E);  idt_set(7,(u32)isr7,0x08,0x8E);
    idt_set(8,(u32)isr8,0x08,0x8E);  idt_set(9,(u32)isr9,0x08,0x8E);
    idt_set(10,(u32)isr10,0x08,0x8E);idt_set(11,(u32)isr11,0x08,0x8E);
    idt_set(12,(u32)isr12,0x08,0x8E);idt_set(13,(u32)isr13,0x08,0x8E);
    idt_set(14,(u32)isr14,0x08,0x8E);idt_set(15,(u32)isr15,0x08,0x8E);
    idt_set(16,(u32)isr16,0x08,0x8E);idt_set(17,(u32)isr17,0x08,0x8E);
    idt_set(18,(u32)isr18,0x08,0x8E);idt_set(19,(u32)isr19,0x08,0x8E);
    idt_set(20,(u32)isr20,0x08,0x8E);idt_set(21,(u32)isr21,0x08,0x8E);
    idt_set(22,(u32)isr22,0x08,0x8E);idt_set(23,(u32)isr23,0x08,0x8E);
    idt_set(24,(u32)isr24,0x08,0x8E);idt_set(25,(u32)isr25,0x08,0x8E);
    idt_set(26,(u32)isr26,0x08,0x8E);idt_set(27,(u32)isr27,0x08,0x8E);
    idt_set(28,(u32)isr28,0x08,0x8E);idt_set(29,(u32)isr29,0x08,0x8E);
    idt_set(30,(u32)isr30,0x08,0x8E);idt_set(31,(u32)isr31,0x08,0x8E);
    /* PIC remap */
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0x0);  outb(0xA1,0x0);
    idt_set(32,(u32)irq0,0x08,0x8E); idt_set(33,(u32)irq1,0x08,0x8E);
    idt_set(34,(u32)irq2,0x08,0x8E); idt_set(35,(u32)irq3,0x08,0x8E);
    idt_set(36,(u32)irq4,0x08,0x8E); idt_set(37,(u32)irq5,0x08,0x8E);
    idt_set(38,(u32)irq6,0x08,0x8E); idt_set(39,(u32)irq7,0x08,0x8E);
    idt_set(40,(u32)irq8,0x08,0x8E); idt_set(41,(u32)irq9,0x08,0x8E);
    idt_set(42,(u32)irq10,0x08,0x8E);idt_set(43,(u32)irq11,0x08,0x8E);
    idt_set(44,(u32)irq12,0x08,0x8E);idt_set(45,(u32)irq13,0x08,0x8E);
    idt_set(46,(u32)irq14,0x08,0x8E);idt_set(47,(u32)irq15,0x08,0x8E);
    ip.limit=sizeof(idt)-1; ip.base=(u32)&idt;
    idt_flush((u32)&ip);
    kprintf("[IDT] inizializzata, PIC remappato\n");
}
