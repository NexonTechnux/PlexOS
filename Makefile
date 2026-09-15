CC=gcc
AS=nasm
CFLAGS=-m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib -nostartfiles -nodefaultlibs \
 -Wall -Wextra -O2 -Iinclude -fno-omit-frame-pointer -march=i386
LDFLAGS=-m elf_i386 -T linker.ld

OBJS=boot/boot.o \
 kernel/kernel.o kernel/gdt.o kernel/idt.o kernel/pmm.o kernel/paging.o kernel/heap.o kernel/timer.o kernel/cpu.o kernel/power.o kernel/mb2.o \
 drivers/serial.o drivers/fb.o drivers/keyboard.o drivers/mouse.o drivers/pci.o drivers/ata.o drivers/rtc.o drivers/gpu.o drivers/e1000.o drivers/bochs.o drivers/intel_disp.o \
 lib/string.o lib/printf.o lib/font.o \
 fs/vfs.o fs/shell.o \
 gui/gui.o \
 apps/terminal.o apps/fileman.o apps/calc.o apps/calc_engine.o apps/browser.o apps/editor.o apps/sysmon.o

all: plexos.iso

boot/boot.o: boot/boot.asm
	$(AS) -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS)
	ld $(LDFLAGS) -o $@ $(OBJS)
	@echo "kernel.bin creato:" && ls -lh kernel.bin

iso/boot/kernel.bin: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/kernel.bin

plexos.iso: iso/boot/kernel.bin iso/boot/grub/grub.cfg
	grub-mkrescue -o plexos.iso iso/ 2>&1 | tail -n 20
	@echo "ISO creata:" && ls -lh plexos.iso

clean:
	rm -f $(OBJS) kernel.bin iso/boot/kernel.bin plexos.iso

run: plexos.iso
	qemu-system-i386 -cdrom plexos.iso -m 256M \
	 -vga std -display gtk,zoom-to-fit=on \
	 -device ide-hd,drive=disk0 -drive id=disk0,if=ide,format=raw,file=/tmp/plexos_disk.img \
	 -net nic,model=e1000 -net user \
	 -serial stdio -rtc base=localtime

run-nographic: plexos.iso
	qemu-system-i386 -cdrom plexos.iso -m 256M -vga std -display none -serial stdio \
	 -net nic,model=e1000 -net user

.PHONY: all clean run run-nographic
