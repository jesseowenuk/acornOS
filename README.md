# AcornOS

A tiny OS built from scratch, step by step, guided by AI.

## What it does right now
- Boots via a custom x86 bootloader
- Enters 32-bit protected mode with a proper GDT
- Handles CPU exceptions and hardware interrupts via IDT + PIC
- Keyboard input via IRQ1 scancode translation
- VGA text driver with colours, cursor and scrolling
- Interactive shell with built-in commands: help, about, clear

## Roadmap
- [ ] Timer driver - IRQ0, system ticks, clock in the corner
- [ ] Memory manager - heap allocation, dynamic memory
- [ ] Shell commands - echo, uptime, splash screen
- [ ] Serial output - kernel logging to terminal
- [ ] Full documentation - every file, function and concept explained

## Building
```bash
# Dependencies (macOS)
brew install nasm qemu i686-elf-gcc

# Build and run
make run
```

## How it works
Each commit in this repo represents a single step in building the OS.