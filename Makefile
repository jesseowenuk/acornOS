.PHONY: all run clean

CC = i686-elf-gcc
CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-builtin

check-size: kernel.bin
	@SECTORS=$$(( ($$(wc -c < kernel.bin) + 511) / 512 )); \
	echo "Kernel: $$(wc -c < kernel.bin) bytes = $$SECTORS sectors"; \
	if [ $$SECTORS -gt 24 ]; then \
		echo "WARNING: kernel is too big! Increase sector count in boot.asm!"; \
	fi

all: os.img

debug: os.img
	qemu-system-i386 -drive format=raw,file=os.img,if=floppy -d int,cpu_reset -no-reboot 2> debug.log

boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot.bin

kernel.bin: kernel/kernel.c kernel/vga.c kernel/gdt.c kernel/gdt_flush.asm kernel/idt.c kernel/isr.asm kernel/idt_flush.asm kernel/pic.c kernel/keyboard.c kernel/shell.c kernel/timer.c kernel/mem.c kernel/serial.c kernel/pmm.c kernel/paging.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/vga.c -o vga.o
	$(CC) $(CFLAGS) -c kernel/gdt.c -o gdt.o
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o
	$(CC) $(CFLAGS) -c kernel/pic.c -o pic.o
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o keyboard.o
	$(CC) $(CFLAGS) -c kernel/shell.c -o shell.o
	$(CC) $(CFLAGS) -c kernel/timer.c -o timer.o
	$(CC) $(CFLAGS) -c kernel/mem.c -o mem.o
	$(CC) $(CFLAGS) -c kernel/serial.c -o serial.o
	$(CC) $(CFLAGS) -c kernel/pmm.c -o pmm.o
	$(CC) $(CFLAGS) -c kernel/paging.c -o paging.o
	nasm -f elf kernel/gdt_flush.asm -o gdt_flush.o
	nasm -f elf kernel/isr.asm -o isr.o
	nasm -f elf kernel/idt_flush.asm -o idt_flush.o
	i686-elf-ld -o kernel.bin \
		-T kernel/linker.ld \
		--oformat binary \
		-Map kernel.map \
		kernel.o vga.o gdt.o gdt_flush.o idt.o isr.o idt_flush.o \
		pic.o keyboard.o shell.o timer.o mem.o serial.o pmm.o paging.o

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 1440k os.img

run: os.img check-size
	qemu-system-i386 -drive format=raw,file=os.img -serial stdio

clean:
	rm -f *.bin *.o *.img