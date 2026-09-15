; PlexOS Multiboot bootloader - richiede framebuffer 1280x720x32 (720p)
MBALIGN  equ 1<<0
MEMINFO  equ 1<<1
VIDEO    equ 1<<2
FLAGS    equ MBALIGN | MEMINFO | VIDEO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0        ; address fields (unused)
    dd 0                    ; 0 = linear graphics mode
    dd 1280                 ; width (720p)
    dd 720                  ; height
    dd 32                   ; depth

; --- Header Multiboot2 (per GRUB EFI su PC UEFI, es. i3-1115G4) ---
; Solo end tag: nessuna richiesta esplicita. GRUB non fallisce mai il load;
; il framebuffer attivo (GOP via gfxpayload=keep) viene ereditato e passato
; al kernel nel tag info 8. Provato: così il loader accetta sempre.
align 8
mb2_begin:
    dd 0xE85250D6
    dd 0
    dd mb2_end - mb2_begin
    dd -(0xE85250D6 + (mb2_end - mb2_begin))
    dw 0                    ; tag end
    dw 0
    dd 8
mb2_end:

section .bss
align 16
stack_bottom:
    resb 32768              ; 32KB stack
stack_top:

section .text
global _start
global gdt_flush
global idt_flush
extern kmain
extern mb2_main

MB2_MAGIC equ 0x36D76289

; --- ISR/IRQ stubs ---
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
global irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
global irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
extern isr_handler
extern irq_handler

_start:
    cli
    mov esp, stack_top
    cmp eax, MB2_MAGIC
    je .mb2                   ; GRUB EFI (multiboot2): EAX=36D76289, EBX=info
    push ebx                ; multiboot info pointer
    push eax                ; multiboot magic
    call kmain
.hang:
    cli
    hlt
    jmp .hang
.mb2:
    push ebx                ; addr info MB2
    push eax                ; magic MB2
    call mb2_main           ; traduce in MB1 e chiama kmain (non ritorna)
.hang2:
    cli
    hlt
    jmp .hang2

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

%macro ISR_NOERR 1
isr%1:
    cli
    push byte 0
    push byte %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    cli
    push byte %1
    jmp isr_common
%endmacro

%macro IRQ_STUB 2
irq%1:
    cli
    push byte 0
    push byte %2
    jmp irq_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

isr_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call isr_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret

irq_common:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    call irq_handler
    add esp, 4
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret
