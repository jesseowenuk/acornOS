.PHONY: all run clean

# 64-bit cross compiler toolchain
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
AS = nasm

# Compiler flags
# -ffreestanding	 	- no standard library
# -mno-red-zone			- disable red zone (required for kernel interrupts)
# -mno-mmx				- disable MMX (keep it simple)
# -mno-sse				- disable SSE (kernel doesn't use floating point)
# -mno-sse2				- disable SSE2
# -mcmodel=kernel		- generate code for kernel address space
#						  (addresses above 0xFFFFFFFF80000000)
# -O1					- optimisation level 1 (safe for kernel)
CFLAGS =  	-ffreestanding \
			-mno-red-zone \
			-mno-mmx \
			-mno-sse \
			-mno-sse2 \
			-mcmodel=kernel \
			-O1 \
			-Wall \
			-Wextra \
			-fno-builtin \
			-fno-stack-protector \
			-fno-pie \
			-fno-pic 

# Linker flags
LDFLAGS = 	-T kernel/linker.ld \
			--oformat binary \
			-Map kernel.map \
			-nostdlib \
			-z max-page-size=0x1000

# NASM flags for 64-bit ELF objects
ASFLAGS = -f elf64

# NASM flags for flat binary (bootloader)
ASFLAGS_BIN = -f bin

check-size: kernel.bin
	@SECTORS=$$(( ($$(wc -c < kernel.bin) + 511) / 512 )); \
	echo "Kernel: $$(wc -c < kernel.bin) bytes = $$SECTORS sectors"; \
	if [ $$SECTORS -gt 500 ]; then \
		echo "WARNING: kernel is too big!"; \
	fi

all: os.img

boot.bin: boot/bootsect.asm
	$(AS) $(ASFLAGS_BIN) boot/bootsect.asm -o boot.bin

stage2.bin: boot/stage2.asm
	$(AS) $(ASFLAGS_BIN) boot/stage2.asm -o stage2.bin

kernel.bin: kernel/start.asm \
		 	kernel/kernel.c \
			kernel/vga.c \
			kernel/gdt.c \
			kernel/gdt_flush.asm \
			kernel/idt.c \
			kernel/isr.asm \
			kernel/idt_flush.asm \
			kernel/pic.c \
			kernel/keyboard.c \
			kernel/shell.c \
			kernel/timer.c \
			kernel/mem.c \
			kernel/serial.c \
			kernel/pmm.c \
			kernel/paging.c \
			kernel/process.c \
			kernel/switch.asm \
			kernel/scheduler.c \
			kernel/syscall.c \
			kernel/kprintf.c \
			kernel/tss.c \
			kernel/usermode.asm \
			kernel/usermode.c \
			kernel/vfs.c \
			kernel/shadowfs.c \
			kernel/panic.c
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
	$(CC) $(CFLAGS) -c kernel/panic.c -o panic.o
	$(AS) -f elf64 kernel/start.asm -o start.o
	$(AS) -f elf64 kernel/gdt_flush.asm -o gdt_flush.o
	$(AS) -f elf64 kernel/isr.asm -o isr.o
	$(AS) -f elf64 kernel/idt_flush.asm -o idt_flush.o
	$(AS) -f elf64 kernel/switch.asm -o switch.o
	$(AS) -f elf64 kernel/usermode.asm -o usermode_asm.o
	$(LD) $(LDFLAGS) \
	 	-o kernel.bin \
		start.o kernel.o vga.o gdt.o gdt_flush.o idt.o isr.o idt_flush.o \
		pic.o keyboard.o shell.o timer.o mem.o serial.o pmm.o paging.o \
		process.o switch.o scheduler.o syscall.o kprintf.o tss.o \
		usermode.o usermode_asm.o vfs.o shadowfs.o panic.o

os.img: boot.bin stage2.bin kernel.bin check-size
	# Create 10MB disk image
	dd if=/dev/zero of=os.img bs=1M count=10 2>/dev/null

	# Write stage 1 at sector 0
	dd if=boot.bin of=os.img bs=512 seek=0 conv=notrunc 2>/dev/null

	# Write stage 2 at sector 1
	dd if=stage2.bin of=os.img bs=512 seek=1 conv=notrunc 2>/dev/null

	# Write kernel at sector 64
	dd if=kernel.bin of=os.img bs=512 seek=64 conv=notrunc 2>/dev/null

	./tools/write_kernel_size.sh kernel.bin os.img

run: os.img check-size
	qemu-system-x86_64 \
		-drive file=os.img,format=raw,index=0,media=disk \
		-serial stdio \
		-m 256M

clean:
	rm -f *.bin *.o *.img