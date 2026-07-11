.PHONY: all run clean

# --- Toolchain -------------------------------------------------
CC = x86_64-elf-gcc
LD = x86_64-elf-ld
AS = nasm

# --- Architecture ----------------------------------------------
ARCH = x86_64

# --- Directories -----------------------------------------------
BUILD_DIR 		= build
BOOT_DIR		= boot
KERNEL_DIR		= kernel
DRIVERS_DIR		= drivers
FS_DIR			= file_system
APPS_DIR		= apps
INCLUDE_DIR		= include
TOOLS_DIR		= tools

# --- Include paths -------------------------------------------
INCLUDES = -I $(INCLUDE_DIR) -I $(INCLUDE_DIR)/architecture/$(ARCH)

# --- Compiler flags ------------------------------------------
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
			-fno-pic \
			$(INCLUDES)

# --- Linker flags -----------------------------------------------
LDFLAGS = 	-T $(KERNEL_DIR)/architecture/$(ARCH)/linker.ld \
			--oformat binary \
			-Map $(BUILD_DIR)/kernel.map \
			-nostdlib \
			-z max-page-size=0x1000

# ---NASM flags ---------------------------------------------------
ASFLAGS = -f elf64

# NASM flags for flat binary (bootloader)
ASFLAGS_BIN = -f bin

# --- Source files ------------------------------------------------

# Bootloader
BOOT_SRCS = $(BOOT_DIR)/bootsect.asm \
			$(BOOT_DIR)/stage2.asm

# Kernel core
KERNEL_C_SRCS = \
	$(KERNEL_DIR)/core/kernel.c \
	$(KERNEL_DIR)/core/kprintf.c \
	$(KERNEL_DIR)/core/panic.c \
	$(KERNEL_DIR)/memory/mem.c \
	$(KERNEL_DIR)/memory/pmm.c \
	$(KERNEL_DIR)/processes/process.c \
	$(KERNEL_DIR)/processes/scheduler.c \
	$(KERNEL_DIR)/processes/syscall.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/gdt.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/idt.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/pic.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/tss.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/paging.c \
	$(KERNEL_DIR)/architecture/$(ARCH)/usermode.c \
	$(KERNEL_DIR)/core/elf.c

KERNEL_ASM_SRCS = \
	$(KERNEL_DIR)/architecture/$(ARCH)/start.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/gdt_flush.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/idt_flush.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/isr.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/switch.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/usermode.asm \
	$(KERNEL_DIR)/architecture/$(ARCH)/syscall_entry.asm

# Drivers
DRIVER_SRCS = \
	$(DRIVERS_DIR)/display/vga.c \
	$(DRIVERS_DIR)/input/keyboard.c \
	$(DRIVERS_DIR)/timer/timer.c \
	$(DRIVERS_DIR)/serial/serial.c \
	$(DRIVERS_DIR)/null/null.c \
	$(DRIVERS_DIR)/random/random.c

# File systems
FS_SRCS = \
	$(FS_DIR)/vfs/vfs.c \
	$(FS_DIR)/shadowfs/shadowfs.c \
	$(FS_DIR)/devfs/devfs.c \
	$(FS_DIR)/procfs/procfs.c

# Apps
APP_SRCS = \
	$(APPS_DIR)/shell/shell.c

# --- Object files --------------------------------------------
KERNEL_C_OBJS		= $(patsubst %.c, 	$(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS		= $(patsubst %.asm, $(BUILD_DIR)/%.asm.o, $(KERNEL_ASM_SRCS))
DRIVER_OBJS			= $(patsubst %.c, 	$(BUILD_DIR)/%.o, $(DRIVER_SRCS))
FS_OBJS				= $(patsubst %.c, 	$(BUILD_DIR)/%.o, $(FS_SRCS))
APP_OBJS			= $(patsubst %.c, 	$(BUILD_DIR)/%.o, $(APP_SRCS))

ALL_OBJS = 	$(KERNEL_ASM_OBJS) \
			$(KERNEL_C_OBJS) \
			$(DRIVER_OBJS) \
			$(FS_OBJS) \
			$(APP_OBJS)

# --- Build rules ------------------------------------------------

all: $(BUILD_DIR)/os.img

# Create build directory structure
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# --- Bootloader ------------------------------------------------

$(BUILD_DIR)/boot.bin: $(BOOT_DIR)/bootsect.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(BUILD_DIR)/stage2.bin: $(BOOT_DIR)/stage2.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Kernel binary ---------------------------------------------
$(BUILD_DIR)/kernel.bin: $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# --- Size check ------------------------------------------------
check-size: $(BUILD_DIR)/kernel.bin
	@SECTORS=$$(( ($$(wc -c < $(BUILD_DIR)/kernel.bin) + 511) / 512 )); \
	echo "Kernel: $$(wc -c < $(BUILD_DIR)/kernel.bin) bytes = $$SECTORS sectors"; \
	if [ $$SECTORS -gt 500 ]; then \
		echo "WARNING: kernel is getting large!"; \
	fi

# --- User programs -----------------------------------------------
# Compiled as standalone ELF64 binaries against acornlibc (libc/)
# Linked at user space virtual address 0x400000

LIBC_DIR = libc

LIBC_SRCS = \
	$(LIBC_DIR)/src/crt0.c \
	$(LIBC_DIR)/src/syscall.c \
	$(LIBC_DIR)/src/string.c \
	$(LIBC_DIR)/src/stdio.c

USER_CC_FLAGS = \
	-ffreestanding \
	-nostdlib \
	-mno-red-zone \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-O1 \
	-Wall \
	-Wextra \
	-fno-builtin \
	-fno-stack-protector \
	-fno-pie \
	-fno-pic \
	-I $(LIBC_DIR)/include

$(BUILD_DIR)/apps/hello/hello.elf: apps/hello/hello.c $(LIBC_SRCS)
	@mkdir -p $(BUILD_DIR)/apps/hello
	$(CC) $(USER_CC_FLAGS) \
	-e _start \
	-Ttext 0x400000 \
	-o $(BUILD_DIR)/apps/hello/hello.elf \
	apps/hello/hello.c $(LIBC_SRCS)

# --- Disk image --------------------------------------------------
$(BUILD_DIR)/os.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage2.bin \
 					 $(BUILD_DIR)/kernel.bin \
					 $(BUILD_DIR)/apps/hello/hello.elf check-size
	# Create 10MB disk image
	dd if=/dev/zero of=$(BUILD_DIR)/os.img bs=1M count=10 2>/dev/null

	# Write stage 1 at sector 0
	dd if=$(BUILD_DIR)/boot.bin of=$(BUILD_DIR)/os.img bs=512 seek=0 conv=notrunc 2>/dev/null

	# Write stage 2 at sector 1
	dd if=$(BUILD_DIR)/stage2.bin of=$(BUILD_DIR)/os.img bs=512 seek=1 conv=notrunc 2>/dev/null

	# Write kernel at sector 64
	dd if=$(BUILD_DIR)/kernel.bin of=$(BUILD_DIR)/os.img bs=512 seek=64 conv=notrunc 2>/dev/null

	$(TOOLS_DIR)/write_kernel_size.sh $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/os.img

	# Write hello.elf at sector 512
	dd if=$(BUILD_DIR)/apps/hello/hello.elf of=$(BUILD_DIR)/os.img bs=512 seek=512 conv=notrunc 2>/dev/null

# --- Run --------------------------------------------------------------------------
run: $(BUILD_DIR)/os.img check-size
	qemu-system-x86_64 \
		-drive file=$(BUILD_DIR)/os.img,format=raw,index=0,media=disk \
		-serial stdio \
		-m 256M \
		-cpu qemu64,+rdrand

clean:
	rm -rf $(BUILD_DIR)

