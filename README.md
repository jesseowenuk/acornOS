# AcornOS

A tiny OS built from scratch, step by step, guided by AI.
Every single line of code written in a single conversation.

## What it does right now
- Boots via a custom x86 bootloader with E820 memory detection
- Enters 32-bit protected mode with a proper GDT
- Handles CPU exceptions and hardware interrupts via IDT + PIC
- Physcial memory manager with E820 map and page bitmap
- Virtual memory manager with paging, page faults and per-process page directories
- VGA text driver with colours, cursor and scrolling
- Keyboard input with process blocking and waking
- PIT timer at 100Hz with live uptime clock
- UART serial driver logging kernel output to terminal via COM1
- Heap memory manager with kmalloc, kfree and block coalescing
- Preemptive round-robin scheduler with context switching
- Kernel and user mode (ring 3) processes
- TSS for ring 3 to ring 0 transitions
- System calls via INT 0x80
- kprintf for formatted kernel output
- Interactive shell with commands: help, about, clear, echo, uptime, mem and ps

## Roadmap
- [ ] Fork and exec
- [ ] ELF loader
- [ ] Filesystem (VFS, ramfs, FAT32)
- [ ] Networking (TCP/IP)
- [ ] Graphics (VESA framebuffer)
- [ ] Desktop environment
- [ ] 64-bit migration
- [ ] AcornBrowser

## Building
```bash
# Dependencies (macOS)
brew install nasm qemu i686-elf-gcc

# Build and run
make run
```

## How it works
Each commit represents a single step in building the OS.