.PHONY: all run clean

CC = i686-elf-gcc
CFLAGS = -ffreestanding -O1 -Wall -Wextra -fno-builtin

check-size: kernel.bin
	@SECTORS=$$(( ($$(wc -c < kernel.bin) + 511) / 512 )); \
	echo "Kernel: $$(wc -c < kernel.bin) bytes = $$SECTORS sectors"; \
	if [ $$SECTORS -gt 120 ]; then \
		echo "WARNING: kernel is too big! Increase sector count in boot.asm!"; \
	fi

all: os.img

debug: os.img
	qemu-system-i386 -drive format=raw,file=os.img,if=floppy -d int,cpu_reset -no-reboot 2> debug.log

boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot.bin

kernel.bin: kernel/start.asm kernel/kernel.c kernel/vga.c kernel/gdt.c kernel/gdt_flush.asm kernel/idt.c \
			kernel/isr.asm kernel/idt_flush.asm kernel/pic.c kernel/keyboard.c \
			kernel/shell.c kernel/timer.c kernel/mem.c kernel/serial.c kernel/pmm.c \
			kernel/paging.c kernel/process.c kernel/switch.asm kernel/scheduler.c \
			kernel/syscall.c kernel/kprintf.c kernel/tss.c kernel/usermode.asm \
			kernel/usermode.c kernel/vfs.c kernel/shadowfs.c
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
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o scheduler.o
	$(CC) $(CFLAGS) -c kernel/syscall.c -o syscall.o
	$(CC) $(CFLAGS) -c kernel/kprintf.c -o kprintf.o
	$(CC) $(CFLAGS) -c kernel/tss.c -o tss.o
	$(CC) $(CFLAGS) -c kernel/usermode.c -o usermode.o
	$(CC) $(CFLAGS) -c kernel/vfs.c -o vfs.o
	$(CC) $(CFLAGS) -c kernel/shadowfs.c -o shadowfs.o
	$(CC) $(CFLAGS) -c kernel/process.c -o process.o
	nasm -f elf kernel/start.asm -o start.o
	nasm -f elf kernel/gdt_flush.asm -o gdt_flush.o
	nasm -f elf kernel/isr.asm -o isr.o
	nasm -f elf kernel/idt_flush.asm -o idt_flush.o
	nasm -f elf kernel/switch.asm -o switch.o
	nasm -f elf kernel/usermode.asm -o usermode_asm.o
	i686-elf-ld -o kernel.bin \
		-T kernel/linker.ld \
		--oformat binary \
		-Map kernel.map \
		start.o kernel.o vga.o gdt.o gdt_flush.o idt.o isr.o idt_flush.o \
		pic.o keyboard.o shell.o timer.o mem.o serial.o pmm.o paging.o \
		process.o switch.o scheduler.o syscall.o kprintf.o tss.o \
		usermode.o usermode_asm.o vfs.o shadowfs.o

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 10M os.img

run: os.img check-size
	qemu-system-i386 -drive file=os.img,format=raw,index=0,media=disk -serial stdio

clean:
	rm -f *.bin *.o *.img