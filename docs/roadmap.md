# acornOS Roadmap
> Living document — updated as milestones are completed.
> Follows acornOS Engineering Principles — see docs/principles.md
> Last updated: June 2026

---

## Legend

```
✅ Complete
🔄 In progress
⬜ Not started
🎮 Fun milestone (game/app/driver)
📄 Documentation milestone
🔧 Tooling milestone
```

---

## Phase 0 — Foundation (COMPLETE ✅)

> The absolute basics. Bootloader, protected mode, kernel entry.

- ✅ Stage 1 bootloader (512 bytes, real mode)
- ✅ E820 memory detection
- ✅ LBA disk loading
- ✅ Protected mode entry
- ✅ GDT (Global Descriptor Table)
- ✅ Kernel entry point (start.asm)
- ✅ BSS zeroing
- ✅ Stack setup
- ✅ kernel_main called with memory map

---

## Phase 1 — Core Kernel (COMPLETE ✅)

> Interrupts, memory, basic I/O.

- ✅ IDT (Interrupt Descriptor Table)
- ✅ PIC (Programmable Interrupt Controller)
- ✅ ISR stubs (exceptions 0-19)
- ✅ IRQ handlers (0-15)
- ✅ Page fault handler
- ✅ VGA text mode driver
- ✅ PS/2 keyboard driver
- ✅ PIT timer at 100Hz
- ✅ UART serial driver (COM1)
- ✅ kprintf / kserial_printf
- ✅ Heap allocator (kmalloc/kfree)
- ✅ PMM (Physical Memory Manager)
- ✅ Paging (identity mapped first 4MB)
- ✅ kpanic and KASSERT

---

## Phase 2 — Process Management (COMPLETE ✅)

> Processes, scheduling, user mode, system calls.

- ✅ Process control block (PCB)
- ✅ Per-process page directories
- ✅ Round-robin preemptive scheduler
- ✅ Context switching (switch.asm)
- ✅ TSS (Task State Segment)
- ✅ Ring 3 user mode processes
- ✅ iret_to_usermode
- ✅ System calls (INT 0x80)
- ✅ SYS_EXIT
- ✅ SYS_WRITE
- ✅ SYS_READ
- ✅ SYS_GETPID
- ✅ SYS_YIELD
- ✅ SYS_FORK
- ✅ SYS_WAIT
- ✅ SYS_EXEC
- ✅ fork() with deep copy of page directory
- ✅ wait() with parent blocking
- ✅ exec() replacing process image
- ✅ Shell process
- ✅ Idle process

---

## Phase 3 — Virtual Filesystem (COMPLETE ✅)

> VFS abstraction layer. Every filesystem plugs into this.

- ✅ VFS design and data structures
- ✅ vfs_init()
- ✅ vfs_mount()
- ✅ vfs_find_mount()
- ✅ vfs_resolve_path()
- ✅ vfs_alloc_fd() / vfs_free_fd() / vfs_get_file()
- ✅ vfs_open()
- ✅ vfs_close()
- ✅ vfs_read()
- ✅ vfs_write()
- ✅ vfs_seek()
- ✅ vfs_mkdir()
- ✅ vfs_readdir()
- ✅ vfs_delete()

---

## Phase 4 — shadowFS (IN PROGRESS 🔄)

> RAM filesystem. Files vanish on power off like shadows in sunlight.

- ✅ shadowFS design (quota, 4KB blocks, linked list dirs)
- ✅ shadowfs_mount() with quota check
- ✅ shadowfs_create_inode()
- ✅ shadowfs_lookup()
- ✅ shadowfs_create()
- ✅ vfs_open() creating files in shadowFS
- ✅ shadowfs_write()
- ✅ shadowfs_read()
- ✅ shadowfs_readdir()
- ✅ shadowfs_mkdir()
- ✅ shadowfs_delete()
- ✅ shadowfs_close()
- ⬜ shadowfs_stats() — usage reporting
- ✅ Mount /temp
- ⬜ Mount /devices (devFS)
- ⬜ Mount /process (procFS)

---

## Phase 5 — 64-bit Migration (NEXT ⬜)

> Migrate everything to x86_64 long mode.
> Do this NOW before the kernel gets bigger.
> See docs/memory_map.md for full details.

### 5a — Toolchain
- ✅ Install x86_64-elf-gcc cross compiler
- ✅ Install x86_64-elf-ld
- ✅ Update Makefile for 64-bit targets

### 5b — Two-Stage Bootloader
- ✅ Stage 1 (512 bytes):
  - ✅ Save boot drive
  - ✅ LBA load Stage 2 (sectors 1-63)
  - ✅ Jump to Stage 2
- ✅ Stage 2 (up to 31.5KB):
  - ✅ Enable A20 line (properly!)
  - ✅ E820 memory detection
  - ✅ Read kernel size dynamically from disk
  - ✅ Load kernel to 0x100000 (any size)
  - ✅ Set up temporary 32-bit GDT
  - ✅ Enter protected mode
  - ✅ Set up 64-bit page tables:
    - ✅ Identity map first 2MB (temporary)
    - ✅ Map kernel: 0xFFFFFFFF80100000 -> 0x100000
    - ✅ Map direct physical map: 0xFFFF800000000000 -> 0x0
    - ✅ Map VGA: 0xFFFFFFFF800B8000 -> 0xB8000
  - ✅ Enable PAE
  - ✅ Enable long mode (EFER.LME)
  - ✅ Enable paging
  - ✅ Far jump to 64-bit code segment
  - ✅ Jump to kernel at 0xFFFFFFFF80100000

### 5c — Kernel Entry (start.asm rewrite)
- ✅ 64-bit kernel entry point
- ✅ Set up kernel stack
- ✅ Zero BSS
- ✅ Extend direct physical map to cover ALL RAM
- ⬜ Remove temporary identity map
- ✅ Set up proper 64-bit GDT
- ✅ Set up 64-bit IDT
- ✅ Call kernel_main

### 5d — Core Kernel Updates
- ✅ Update linker script (. = 0xFFFFFFFF80100000)
- ✅ Update GDT for 64-bit
- ✅ Update IDT for 64-bit
- ✅ Update ISR stubs for 64-bit (isr.asm)
- ✅ Update context switch (switch.asm)
- ✅ Update TSS for 64-bit
- ✅ Update syscall mechanism (keep INT 0x80 for now)
- ✅ Update all uint32_t addresses to uint64_t
- ✅ Update VGA driver (new virtual address)
- ✅ Update serial driver

### 5e — Memory Management Updates
- ✅ PMM rewrite for 64-bit
  - ✅ Dynamic bitmap sizing from E820
  - ✅ Support up to 512MB RAM
  - ✅ Reserve ALL regions before first allocation
- ✅ Paging rewrite for 4-level paging
  - ✅ PML4 / PDPT / PD / PT structures
  - ✅ Direct physical map (ALL RAM)
  - ✅ 2MB large pages for direct map
  - ✅ map_page() for 64-bit
  - ✅ map_page_in() for 64-bit
- ✅ Heap update (new virtual address)

### 5f — Process Management Updates
- ✅ Update process_t for 64-bit registers
- ✅ Update cpu_state_t (64-bit registers)
- ✅ Update fork() for 64-bit
- ✅ Update exec() for 64-bit
- ⬜ Update user mode entry (iret_to_usermode)
- ⬜ Update user stack address (USER_STACK_TOP)

### 5g — Verification
- ✅ Boot to kernel_main in 64-bit
- ✅ Serial output working
- ✅ VGA output working
- ✅ Interrupts working
- ✅ Keyboard working
- ✅ Shell working
- ✅ fork/exec/wait working
- ✅ VFS and shadowFS working
- ✅ All previous tests passing

---

## Phase 6 — shadowFS Completion (⬜)

> Complete shadowFS after 64-bit migration.

- ✅ shadowfs_write() (with 64-bit PMM)
- ✅ shadowfs_read()
- ✅ shadowfs_readdir()
- ✅ shadowfs_mkdir()
- ✅ shadowfs_delete()
- ✅ Mount /temp
- ⬜ Mount /devices
- ⬜ Mount /process
- ✅ Wire VFS syscalls (SYS_OPEN, SYS_READ, SYS_WRITE, SYS_CLOSE)
- ✅ Shell ls command (uses readdir)
- ✅ Shell cat command (uses read)
- ✅ Shell echo command (uses write)
- ✅ Shell mkdir command
- ✅ Shell rm command

---

## Phase 7 — devFS (⬜)

> Device files. Everything is a file.

- ⬜ devFS design and implementation
- ⬜ /devices/keyboard (read = get keypress)
- ⬜ /devices/display (write = print to screen)
- ⬜ /devices/serial (read/write serial port)
- ⬜ /devices/null (write = discard, read = EOF)
- ⬜ /devices/random (read = random bytes)
- ⬜ /devices/mem (read/write physical memory)
- ⬜ Shell: cat /devices/keyboard
- ⬜ Shell: echo "hello" > /devices/display

---

## Phase 8 — procFS (⬜)

> Process information as files. Live kernel data.

- ⬜ procFS design and implementation
- ⬜ /process/[pid]/status
- ⬜ /process/[pid]/memory
- ⬜ /process/[pid]/files
- ⬜ /process/meminfo (total/free/used RAM)
- ⬜ /process/mounts (mounted filesystems)
- ⬜ Shell ps command uses procFS
- ⬜ Shell mem command uses procFS

---

## Phase 9 — ELF Loader (⬜)

> Load real compiled programs from disk.

- ⬜ ELF64 format understanding
- ⬜ ELF header parser
- ⬜ Program header parser
- ⬜ Segment loader (LOAD segments)
- ⬜ BSS zeroing for ELF
- ⬜ Entry point extraction
- ⬜ exec() updated to load ELF files
- ⬜ Shell can run ELF binaries
- ⬜ First compiled C program runs on acornOS!

---

## Phase 10 — Basic libc (⬜)

> Minimal C library for user space programs.

- ⬜ libc design (acornlibc)
- ⬜ syscall wrappers (open, read, write, close, exit)
- ⬜ printf (uses SYS_WRITE)
- ⬜ malloc / free (uses brk syscall)
- ⬜ string functions (strlen, strcpy, strcmp etc.)
- ⬜ Basic file I/O (fopen, fclose, fread, fwrite)
- ⬜ Hello World compiles and runs!

---

## Phase 11 — First Games! (⬜)

> Text mode games. The first real acornOS applications.

- ⬜ 🎮 Snake
  - ⬜ VGA character graphics
  - ⬜ Keyboard input
  - ⬜ Score tracking
  - ⬜ Runs via fork/exec from shell
- ⬜ 🎮 Tetris
  - ⬜ Rotation and collision
  - ⬜ Line clearing
  - ⬜ Score and levels
- ⬜ 🎮 Pong
  - ⬜ Two player
  - ⬜ Simple AI opponent
- ⬜ 🎮 Minesweeper
- ⬜ 🎮 Chess (with AI)
- ⬜ 🎮 Roguelike dungeon crawler

---

## Phase 12 — ATA/IDE Driver (⬜)

> Talk to real disk hardware.

- ⬜ PCI subsystem (enumerate devices)
- ⬜ ATA/IDE driver
- ⬜ Read/write sectors
- ⬜ MBR partition table parser
- ⬜ Disk abstraction layer

---

## Phase 13 — FAT32 (⬜)

> Read USB sticks and disk images from host machine.

- ⬜ FAT32 filesystem driver
- ⬜ Mount FAT32 partition
- ⬜ Read files from FAT32
- ⬜ Write files to FAT32
- ⬜ ls, cat, cp working on FAT32
- ⬜ 🎮 Load game assets from USB stick!

---

## Phase 14 — barkFS v1 (⬜)

> Our own persistent filesystem. The bark of the tree — always present.

- ⬜ barkFS design
  - ⬜ Content addressable storage
  - ⬜ Built-in versioning from day one
  - ⬜ Crash safe (atomic writes)
  - ⬜ Metadata support
- ⬜ On-disk format specification
- ⬜ barkFS driver
- ⬜ Mount as root filesystem
- ⬜ Basic file operations
- ⬜ Directory operations
- ⬜ Versioning (read file@v1, file@v2 etc.)
- ⬜ 🎮 Save game progress to barkFS!

---

## Phase 15 — VESA/VBE Graphics (⬜)

> Pixel graphics. The world gets colourful.

- ⬜ VESA BIOS Extensions (VBE) mode setting
- ⬜ Framebuffer driver
- ⬜ 2D drawing primitives:
  - ⬜ Plot pixel
  - ⬜ Draw line (Bresenham)
  - ⬜ Draw rectangle
  - ⬜ Fill rectangle
  - ⬜ Draw circle
  - ⬜ Blit bitmap
- ⬜ Double buffering
- ⬜ Bitmap font rendering
- ⬜ PNG/BMP image loading
- ⬜ 🎮 Graphical Snake!
- ⬜ 🎮 Raycaster (Doom-style 3D!)
- ⬜ 🎮 Sprite platformer
- ⬜ 🎮 Space Invaders
- ⬜ 🎮 Breakout/Arkanoid
- ⬜ 🎮 Scrolling RPG (Zelda-style)
- ⬜ Screensaver (bouncing acorn logo)

---

## Phase 16 — PS/2 Mouse (⬜)

> Mouse support. Click things.

- ⬜ PS/2 mouse driver
- ⬜ Mouse cursor rendering
- ⬜ Mouse events in shell
- ⬜ /devices/mouse device file
- ⬜ 🎮 Mouse-controlled games

---

## Phase 17 — Networking (⬜)

> Connect to the world.

- ⬜ PCI network card detection
- ⬜ RTL8139 driver (common in QEMU)
- ⬜ virtio-net driver (QEMU optimised)
- ⬜ Ethernet layer
- ⬜ ARP
- ⬜ IPv4
- ⬜ ICMP (ping!)
- ⬜ UDP
- ⬜ TCP
- ⬜ DNS resolver
- ⬜ DHCP client
- ⬜ Socket API (SYS_SOCKET etc.)
- ⬜ 🎮 ping command — first network contact!
- ⬜ 🎮 Multiplayer Pong over network!
- ⬜ 🎮 Multiplayer Snake!
- ⬜ 🎮 Simple HTTP server
- ⬜ wget / curl equivalent

---

## Phase 18 — Audio (⬜)

> Sound! Music! Your MIDI keyboard!

- ⬜ PC speaker driver (simple beeps)
- ⬜ AC97 audio driver
- ⬜ Intel HDA audio driver
- ⬜ PCM audio output
- ⬜ WAV file player
- ⬜ MP3 decoder
- ⬜ Chiptune player
- ⬜ 🎮 Sound effects in games!
- ⬜ 🎮 Background music in games!

### MIDI Support (Your Keyboard!)
- ⬜ MIDI input driver
- ⬜ MIDI output driver
- ⬜ Software synthesiser
  - ⬜ FM synthesis
  - ⬜ Wavetable synthesis
- ⬜ MIDI sequencer
- ⬜ Piano roll editor
- ⬜ 🎹 Play your MIDI keyboard on acornOS!
- ⬜ 🎮 Rhythm game using MIDI input!
- ⬜ 🎵 Multi-track recorder
- ⬜ 🎵 Export to WAV

---

## Phase 19 — USB Stack (⬜)

> USB devices. Your printer!

- ⬜ UHCI/OHCI/EHCI USB host controller
- ⬜ USB hub driver
- ⬜ USB device enumeration
- ⬜ USB keyboard driver
- ⬜ USB mouse driver
- ⬜ USB storage (pen drives!)
- ⬜ USB audio
- ⬜ USB serial/parallel

### Your Printer!
- ⬜ USB printer driver
- ⬜ Print spooler
- ⬜ PCL printer language
- ⬜ PostScript support
- ⬜ PDF generation
- ⬜ 🖨️ Print "Hello from acornOS!" page!
- ⬜ 🖨️ Print ASCII art
- ⬜ lp command (print a file)
- ⬜ Network printing (future)

---

## Phase 20 — Windowing System (⬜)

> Windows, buttons, mouse clicks. A real GUI.

- ⬜ Display server design
- ⬜ Window manager
- ⬜ Widget toolkit (acornUI)
  - ⬜ Window with title bar
  - ⬜ Buttons
  - ⬜ Text input
  - ⬜ Menus
  - ⬜ Scrollbars
  - ⬜ Dialogs
- ⬜ Event system (mouse, keyboard events)
- ⬜ Window decorations
- ⬜ Themes support
- ⬜ 🎮 First window appears on screen!
- ⬜ 🎮 Graphical Snake with proper window!
- ⬜ 🎮 Graphical Tetris!
- ⬜ 🖼️ Simple image viewer
- ⬜ 📝 Simple text editor (acornEdit)

---

## Phase 21 — Desktop Environment (⬜)

> A complete desktop. Daily driveable begins.

- ⬜ Desktop shell
- ⬜ Taskbar / dock
- ⬜ App launcher
- ⬜ System tray
- ⬜ File manager (graphical)
- ⬜ Terminal emulator
- ⬜ Settings app
- ⬜ Themes and wallpapers
- ⬜ 🎮 Games launcher!
- ⬜ 🎵 Music player with visualiser
- ⬜ 🎹 MIDI sequencer GUI
- ⬜ 📸 Photo viewer

---

## Phase 22 — 3D Graphics (⬜)

> Hardware accelerated 3D. The gaming OS vision realised.

### Software Renderer First
- ⬜ 3D software rasteriser from scratch
- ⬜ Perspective correct texture mapping
- ⬜ Z-buffer
- ⬜ Phong shading
- ⬜ 🎮 Quake-style BSP renderer!
- ⬜ 🎮 Simple 3D FPS game!

### Hardware Acceleration
- ⬜ virtio-gpu driver (QEMU)
- ⬜ Intel integrated graphics driver
- ⬜ AMD graphics driver (open source)
- ⬜ acornGL (OpenGL-like API)
  - ⬜ Vertex buffers
  - ⬜ Shaders
  - ⬜ Textures
  - ⬜ Depth testing
  - ⬜ Blending
- ⬜ 🎮 Hardware accelerated 3D games!
- ⬜ 🎮 Racing game!
- ⬜ 🎮 Space sim!

---

## Phase 23 — acornCC (⬜)

> Our own C/C++ compiler. Written from scratch.

- ⬜ C preprocessor
  - ⬜ #include
  - ⬜ #define
  - ⬜ #ifdef / #ifndef
  - ⬜ Macro expansion
- ⬜ Lexer (tokeniser)
- ⬜ Parser (AST builder)
- ⬜ Semantic analysis (type checker)
- ⬜ x86_64 code generator
- ⬜ ARM code generator (future)
- ⬜ Linker
- ⬜ C++ support (future)
- ⬜ 🔧 acornCC compiles Hello World!
- ⬜ 🔧 acornCC compiles Snake!
- ⬜ 🔧 acornCC compiles itself! (self-hosting!)
- ⬜ 🔧 Write acornOS apps in acornOS!

---

## Phase 24 — acornSSL / TLS (⬜)

> Our own TLS implementation. Required for acornBrowser.
> Cannot use OpenSSL — must be written from scratch.

- ⬜ Cryptographic primitives:
  - ⬜ AES-128/256 (with AES-NI hardware)
  - ⬜ SHA-256 / SHA-384 / SHA-512
  - ⬜ RSA
  - ⬜ ECDSA / ECDHE (elliptic curve)
  - ⬜ ChaCha20-Poly1305
  - ⬜ X25519 key exchange
  - ⬜ HMAC
  - ⬜ HKDF
  - ⬜ CSPRNG (using RDRAND)
- ⬜ X.509 certificate parser
- ⬜ Certificate chain validation
- ⬜ Root CA store
- ⬜ TLS 1.2 handshake
- ⬜ TLS 1.3 handshake
- ⬜ TLS record layer
- ⬜ HTTPS support
- ⬜ Certificate pinning
- ⬜ HSTS support
- ⬜ 🔒 First HTTPS connection from acornOS!

---

## Phase 25 — acornBrowser (⬜)

> Our own web browser. The ultimate application.

- ⬜ HTTP/1.1 client
- ⬜ HTTP/2 client
- ⬜ HTTPS (via acornSSL)
- ⬜ HTML5 parser
- ⬜ CSS3 engine
- ⬜ DOM implementation
- ⬜ Layout engine
- ⬜ Rendering engine (via acornGL)
- ⬜ JavaScript engine (acornJS)
  - ⬜ Lexer and parser
  - ⬜ Interpreter
  - ⬜ JIT compiler (future)
- ⬜ WebGL support (3D in browser!)
- ⬜ WebAssembly runtime
- ⬜ Media codecs:
  - ⬜ JPEG / PNG / WebP images
  - ⬜ MP3 / AAC / Opus audio
  - ⬜ H.264 / AV1 video (future)
- ⬜ 🌐 First webpage loads in acornBrowser!
- ⬜ 🎮 Browser games run on acornOS!

---

## Phase 26 — More Filesystems (⬜)

> Compatibility with the world.

- ⬜ ext2 (Linux read support)
- ⬜ ext4 (Linux read/write)
- ⬜ APFS read support (read macOS drives!)
- ⬜ NTFS read support (read Windows drives!)
- ⬜ exFAT (SD cards, large USB)
- ⬜ NFS (network filesystem)
- ⬜ SMB/CIFS (Windows shares)

### barkFS FUSE Drivers
> Mount acornOS disk images on other OSes.
> Drag files in Finder — they appear in acornOS!

- ⬜ barkFS FUSE driver (C library)
- ⬜ macOS app (SwiftUI + macFUSE)
  - ⬜ Menu bar icon (acorn! 🌱)
  - ⬜ Auto-mount on launch
  - ⬜ Appears in Finder sidebar
  - ⬜ Launch at login option
- ⬜ Linux FUSE driver + .desktop file
- ⬜ Windows driver (Dokan/WinFUSE)
  - ⬜ System tray app (C#)
  - ⬜ Appears in Explorer

---

## Phase 27 — barkFS v2 (⬜)

> Full featured versioning filesystem.

- ⬜ Snapshots (save entire filesystem state)
- ⬜ Restore to any snapshot
- ⬜ Metadata queries ("find all images from 2026")
- ⬜ Deduplication (content addressable)
- ⬜ Compression
- ⬜ Encryption
- ⬜ Network replication
- ⬜ barkFS over network (like NFS but ours)

---

## Phase 28 — 64-bit to ARM (⬜)

> Run on more hardware.

- ⬜ ARM64 (AArch64) port
- ⬜ Raspberry Pi 4/5 support
- ⬜ ARM bootloader
- ⬜ ARM paging (similar to x86_64)
- ⬜ ARM interrupt controller (GIC)
- ⬜ ARM drivers
- ⬜ 🎮 Boot on Raspberry Pi!
- ⬜ Apple Silicon support (future)

---

## Phase 29 — Daily Driveable (⬜)

> Use acornOS as your main OS. The ultimate goal.

- ⬜ WiFi driver
- ⬜ Bluetooth driver
- ⬜ Suspend / resume
- ⬜ Multiple monitor support
- ⬜ Hardware detection / plug and play
- ⬜ Installer (install to real hardware)
- ⬜ Package manager (acornPkg)
- ⬜ App store (acornStore)
  - ⬜ Community games
  - ⬜ Community apps
  - ⬜ DRM-free always
- ⬜ Achievements system
- ⬜ Streaming / recording
- ⬜ acornIDE (development environment)
- ⬜ 🏆 Boot on real gaming PC!
- ⬜ 🏆 Browse the web in acornBrowser!
- ⬜ 🏆 Play a 3D game!
- ⬜ 🏆 Play your MIDI keyboard!
- ⬜ 🏆 Print a document!
- ⬜ 🏆 Compile acornOS with acornCC!

Phase 30: More features!!!!
- ⬜ FRED (Flexible Return and Event Delivery)
      Intel only
      Unified interrupt/exception/syscall delivery
      Replace IDT + SYSCALL when hardware is common

---

## Fun Milestone Summary 🎮

> The games, apps and drivers that make this worthwhile.

### Text Mode Games (Phase 11)
- ⬜ 🎮 Snake
- ⬜ 🎮 Tetris
- ⬜ 🎮 Pong
- ⬜ 🎮 Minesweeper
- ⬜ 🎮 Chess with AI
- ⬜ 🎮 Roguelike dungeon crawler

### 2D Games (Phase 15)
- ⬜ 🎮 Graphical Snake
- ⬜ 🎮 Space Invaders
- ⬜ 🎮 Breakout
- ⬜ 🎮 Sprite platformer (Mario-style)
- ⬜ 🎮 Top-down RPG (Zelda-style)
- ⬜ 🎮 Raycaster (Doom-style!)

### Networked Games (Phase 17)
- ⬜ 🎮 Multiplayer Pong
- ⬜ 🎮 Multiplayer Snake
- ⬜ 🎮 Turn-based strategy

### 3D Games (Phase 22)
- ⬜ 🎮 Simple FPS
- ⬜ 🎮 Racing game
- ⬜ 🎮 Space sim

### Music & MIDI (Phase 18)
- ⬜ 🎹 Play MIDI keyboard on acornOS
- ⬜ 🎵 Chiptune player
- ⬜ 🎵 Multi-track recorder
- ⬜ 🎮 Rhythm game with MIDI input

### Hardware (Phases 19, 28, 29)
- ⬜ 🖨️ Print from acornOS via USB cable
- ⬜ 🖥️ Boot on real hardware
- ⬜ 🍓 Boot on Raspberry Pi

### Compiler (Phase 23)
- ⬜ 🔧 acornCC compiles Hello World
- ⬜ 🔧 acornCC compiles itself (self-hosting!)

### Browser (Phase 25)
- ⬜ 🌐 First webpage in acornBrowser
- ⬜ 🎮 Browser games on acornOS

---

## The Ultimate Vision

```
acornOS — built entirely from scratch
         every line documented
         every concept explained
         started with 512 bytes that just hung

         A gamer's OS
         A developer's OS
         A learning OS
         A lifetime project

         When complete:
         ✅ Boots on real gaming hardware
         ✅ 3D games run natively
         ✅ MIDI keyboard works
         ✅ Printer works
         ✅ Browser loads the web
         ✅ acornCC compiles itself
         ✅ Daily driveable

         🌱
```

---

## Change Log

```
June 2026   Initial roadmap
            Phases 0-3 marked complete
            Phase 4 (shadowFS) in progress
            Phase 5 (64-bit migration) identified as next priority
            Gaming OS vision formalised
            Fun milestones added throughout
```
