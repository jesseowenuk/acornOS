# AcornOS

A tiny OS built from scratch, step by step, guided by AI.
Every single line of code written in a single conversation.

## What it does right now

**Boot & kernel core**
- Boots via a custom x86 bootloader with E820 memory detection
- Enters 64-bit long mode with a proper GDT and TSS
- Handles CPU exceptions and hardware interrupts via IDT + PIC
- UART serial driver logging kernel output to terminal via COM1
- VGA text driver with colours, cursor and scrolling
- PIT timer at 100Hz with live uptime clock
- Keyboard input with process blocking and waking
- kprintf/kserial_printf for formatted kernel output

**Memory**
- Physical memory manager with E820 map and page bitmap
- Virtual memory manager with paging, page faults and per-process page directories
- Heap memory manager with kmalloc, kfree and block coalescing

**Processes & syscalls**
- Preemptive round-robin scheduler with context switching
- Kernel and user mode (ring 3) processes
- System calls via SYSCALL/SYSRET (64-bit fast path)
- fork() and wait() with real exit-code round-tripping
- ELF64 loader — exec() loads real ELF binaries into a process's address space
- Per-process kernel stacks and saved user state (syscalls can block safely)

**Filesystems**
- Virtual Filesystem (VFS) layer with file descriptors
- shadowFS — the in-memory writable filesystem
- devFS — /devices/display, /devices/keyboard, /devices/serial, /devices/null, /devices/random
- procFS — /process, exposing live process info
- fd-aware file I/O: open/read/write/close/seek against real files, stdin/stdout/stderr wired up at boot

**User space**
- acornlibc — a minimal C library: crt0 startup, raw syscall wrappers, printf/vprintf, string.h, malloc/free (free-list allocator over a grow-only heap)
- Interactive shell with commands: help, about, clear, echo, uptime, mem, ps, ls, cat, mkdir, rm, run

## Roadmap

The full, actively-maintained roadmap — including everything already shipped and everything still planned — lives in [docs/roadmap.md](docs/roadmap.md).

## Building
```bash
# Dependencies (macOS)
brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils

# Build and run
make run
```

## How it works
Each commit represents a single step in building the OS.
