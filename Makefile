.PHONY: all run clean

CC = i686-elf-gcc
CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-builtin

all: os.img

boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot.bin

kernel.bin: kernel/kernel.c kernel/vga.c kernel/gdt.c kernel/gdt_flush.asm kernel/idt.c kernel/isr.asm kernel/idt_flush.asm
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/vga.c -o vga.o
	$(CC) $(CFLAGS) -c kernel/gdt.c -o gdt.o
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o
	nasm -f elf kernel/gdt_flush.asm -o gdt_flush.o
	nasm -f elf kernel/isr.asm -o isr.o
	nasm -f elf kernel/idt_flush.asm -o idt_flush.o
	i686-elf-ld -o kernel.bin -Ttext 0x1000 --oformat binary kernel.o vga.o gdt.o gdt_flush.o idt.o isr.o idt_flush.o

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	truncate -s 1440k os.img

run: os.img
	qemu-system-i386 -drive format=raw,file=os.img

clean:
	rm -f *.bin *.o *.img