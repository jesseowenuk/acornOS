# AcornOS

A tiny OS built from scratch, step by step, guided by AI.

## What it does right now
- Boots via a custom x86 bootloader
- Enters 32-bit protected mode with a proper GDT
- Handles CPU exceptions and hardware interrupts via IDT + PIC
- Keyboard input via IRQ1 scancode translation
- VGA text driver with colours, cursor and scrolling
- Interactive shell with built-in commands: help, about, clear
- UART serial driver logging kernel output to terminal via COM1

## Roadmap
- [ ] Full documentation - every file, function and concept explained
- [ ] Processes and scheduling
- [ ] Filesystem

## Building
```bash
# Dependencies (macOS)
brew install nasm qemu i686-elf-gcc

# Build and run
make run
```

## How it works
Each commit in this repo represents a single step in building the OS.