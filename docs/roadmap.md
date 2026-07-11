# acornOS Roadmap
> Living document — updated as milestones are completed.
> Follows acornOS Engineering Principles — see docs/principles.md
> Last updated: 11th July 2026

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
- ✅ shadowfs_stats() — usage reporting
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
- ✅ Wire VFS syscalls (SYS_OPEN, SYS_READ, SYS_WRITE, SYS_CLOSE)
- ✅ Shell ls command (uses readdir)
- ✅ Shell cat command (uses read)
- ✅ Shell echo command (uses write)
- ✅ Shell mkdir command
- ✅ Shell rm command

---

## Phase 7 — devFS (⬜)

> Device files. Everything is a file.

- ✅ devFS design and implementation
- ✅ /devices/keyboard (read = get keypress)
- ✅ /devices/display (write = print to screen)
- ✅ /devices/serial (read/write serial port)
- ✅ /devices/null (write = discard, read = EOF)
- ✅ /devices/random (read = random bytes)
- ⬜ /devices/mem (read/write physical memory)
- ⬜ Shell: cat /devices/keyboard
- ⬜ Shell: echo "hello" > /devices/display

---

## Phase 7.5 — Real-Time Clock (⬜)

> Know what time it actually is. Only the 100Hz scheduler tick exists
> right now - no wall-clock time anywhere, so file timestamps
> (created/modified/accessed - already fields on every inode) can't be
> real.

- ⬜ RTC (CMOS clock) driver
- ⬜ Read current date/time at boot
- ⬜ /devices/rtc (read current time)
- ⬜ Real timestamps for shadowFS/barkFS (created/modified/accessed)
- ⬜ Shell `date` command

---

## Phase 8 — procFS (⬜)

> Process information as files. Live kernel data.

- ✅ procFS design and implementation
- ✅ /process/[pid]/status
- ✅ /process/[pid]/memory
- ✅ /process/[pid]/files
- ✅ /process/meminfo (total/free/used RAM)
- ✅ /process/mounts (mounted filesystems)
- ✅ Shell ps command uses procFS
- ✅ Shell mem command uses procFS

---

## Phase 9 — ELF Loader (✅)

> Load real compiled programs from disk.

- ✅ ELF64 format understanding
- ✅ ELF header parser
- ✅ Program header parser
- ✅ Segment loader (LOAD segments)
- ✅ BSS zeroing for ELF
- ✅ Entry point extraction
- ✅ exec() updated to load ELF files
- ✅ Shell can run ELF binaries
- ✅ First compiled C program runs on acornOS!
- ✅ Switch syscalls from INT 0x80 to SYSCALL/SYSRET
- ✅ Update syscall argument convention (rbx→rdi, rcx→rsi)
- ✅ Update hello.c and all user programs

---

## Phase 10 — Basic libc (🔄)

> Minimal C library for user space programs.

- ✅ libc design (acornlibc)
- ✅ syscall wrappers (open, read, write, close, exit - plus getpid,
      yield, fork, wait, exec)
- ✅ printf (uses SYS_WRITE)
- ✅ malloc / free (uses brk syscall)
- ✅ string functions (strlen, strcpy, strcmp etc.)
- ✅ Raw file I/O (open/read/write/close/seek) - fd-aware, real files
- ⬜ Buffered stdio (fopen, fclose, fread, fwrite)
- ✅ Hello World compiles and runs!

---

## Phase 10.5 — Process & Program Fundamentals (⬜)

> The missing pieces every real program expects. Do this before Phase 11
> (games) - Snake/Tetris/Pong all want at least argc/argv and sleep().

- ⬜ Pass argc/argv to exec'd programs (currently main(void) - no arguments)
  - ⬜ Shell parses command line into argv
  - ⬜ exec()/process_spawn() pass argv through to the new process
  - ⬜ crt0 unpacks argv before calling main(int argc, char** argv)
- ⬜ Environment variables
  - ⬜ getenv() / setenv() in libc
  - ⬜ Environment block passed at exec time
  - ⬜ PATH variable - shell searches it for commands
- ⬜ SYS_SLEEP - block a process for N milliseconds
  - ⬜ sleep() / usleep() in libc
  - ⬜ Timer-driven wakeup (not busy-waiting)
- ⬜ Per-process file descriptor tables
  - ⬜ Move file_table from a single global array into process_t
        (file_system/vfs/vfs.c already says "For now a global table -
        later each process gets its own")
  - ⬜ fork() copies the parent's fd table
- ⬜ TTY line discipline
  - ⬜ Shared canonical-mode input handling (backspace, line buffering) -
        currently reimplemented ad-hoc inside the shell
  - ⬜ Raw mode (for games reading single keypresses without Enter)

---

## Phase 10.6 — Process Lifecycle, Signals & Safety (⬜)

> Making sure a broken process is a contained problem, not a hung
> system - worth having before games exist to crash.

- ⬜ Complete process_exit() cleanup
  - ⬜ Free a process's stack/page directory/PCB even if nothing ever
        calls wait() on it (currently only reclaimed via process_wait())
- ⬜ Kernel stack guard pages
  - ⬜ Leave an unmapped page below each process's kernel stack so a
        stack overflow page-faults cleanly instead of silently
        corrupting adjacent memory
- ⬜ Signals
  - ⬜ SIGKILL - force-terminate a process
  - ⬜ SIGSEGV - delivered on a page/GP fault instead of hanging
  - ⬜ SIGINT - Ctrl+C support
  - ⬜ Default handlers (terminate, ignore) + basic signal() registration
- ⬜ Shell: Ctrl+C interrupts the running foreground program
- ⬜ Kernel panic improvements
  - ⬜ Stack backtrace on panic (GDB doesn't work on this hardware - see
        docs/CLAUDE_CODE_HANDOFF.md - so this is the primary debugging
        tool going forward)
  - ⬜ Graceful recovery vs hard halt where possible

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

## Phase 11.5 — Kernel Robustness & Testing (⬜)

> Verification has been 100% manual (build, boot in QEMU, read the
> serial log) for every change so far. That's fine today; it won't
> scale as acornOS grows. Do this once there's something fun (Phase 11)
> to protect, before tackling bigger subsystems (disk drivers, real
> filesystems) that are much harder to debug by hand.

- ⬜ Automated boot-test harness (script QEMU + assert on serial output)
- ⬜ Regression test suite - one test per fixed bug at minimum
- ⬜ CI pipeline (run the test suite on every commit)
- ⬜ Full x86_64 exception table coverage audit (all 32 CPU exceptions
      handled explicitly, not just the handful wired up so far)
- ⬜ pmm_alloc() fails gracefully instead of panicking the kernel
      (tracked separately as its own task)
- ⬜ Audit every other allocation call site for the same assumption
      ("this can never fail")

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

## Phase 13.5 — Advanced Memory Management (⬜)

> Needs a real disk (Phase 12/13) for swap space to make sense.

- ⬜ Copy-on-write fork()
  - ⬜ Share physical pages between parent/child, marked read-only
  - ⬜ Page fault handler copies-on-write when either side writes
  - ⬜ Replaces today's full deep-copy fork (works, but O(process size)
        in time and memory on every single fork)
- ⬜ Demand paging (map pages lazily, not all up front)
- ⬜ Swap to disk (page out when physical memory runs low)
- ⬜ Memory-mapped files (mmap) - map a file's contents directly into
      a process's address space

---

## Phase 13.6 — Inter-Process Communication (⬜)

> `ls | grep` isn't possible yet - no way for processes to talk to each
> other beyond fork/exec/wait exit codes.

- ⬜ Pipes (anonymous, for shell `|` support)
- ⬜ Named pipes / FIFOs (via VFS)
- ⬜ Shared memory segments
- ⬜ Shell: pipe support (`cmd1 | cmd2`)

---

## Phase 13.7 — Threads & Synchronization (⬜)

> Multiple execution contexts sharing one address space - and the
> primitives to coordinate them safely, needed by IPC (13.6) and every
> concurrent subsystem after this point.

- ⬜ Kernel-level threads (share page directory, separate stack/registers)
- ⬜ Thread-local storage (TLS)
- ⬜ Mutexes
- ⬜ Condition variables
- ⬜ Spinlocks (for use inside the kernel itself)
- ⬜ pthread-style libc API (thread_create, mutex_lock etc.)

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

## Phase 17.5 — FPU / SIMD Support & Math Library (⬜)

> No floating point context switching exists at all right now - SSE/MMX
> are deliberately disabled for every user program (-mno-sse -mno-mmx)
> to avoid corrupting state across context switches. Audio/MIDI (18) and
> 3D graphics (22) both need real floating point math - this phase is a
> hard prerequisite for both.

- ⬜ FXSAVE/FXRSTOR-based FPU state save/restore per process
- ⬜ Re-enable SSE/SSE2 for user programs safely
- ⬜ libm - sin, cos, sqrt, pow, floor, ceil etc.
- ⬜ Verify float/double math works correctly across context switches
  under scheduler preemption (the actual risk this phase exists to fix)

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

## Phase 20.5 — Dynamic Linking & Shared Libraries (⬜)

> Every program currently statically links the whole of acornlibc.
> Fine while there's one small libc; won't scale once acornUI (20) and
> bigger programs exist - and updating libc currently means recompiling
> every program that uses it.

- ⬜ Shared library format (ELF .so-equivalent)
- ⬜ Dynamic linker/loader
- ⬜ PLT/GOT (lazy symbol resolution)
- ⬜ acornlibc as a shared library
- ⬜ ldd-style tool (list a binary's dependencies)

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

## Phase 27.5 — SMP / Multi-core Support (⬜)

> The kernel already has a standing TODO for this (per-CPU data was
> called out explicitly while fixing the syscall entry path - see
> syscall_entry.asm history) - it's referenced in the code without
> being a tracked phase anywhere. Best tackled once single-core acornOS
> is very mature, since it touches almost everything: scheduler, locks,
> memory management.

- ⬜ ACPI/MP table parsing (discover other CPU cores)
- ⬜ AP (application processor) bootstrap
- ⬜ Per-CPU data structures (replacing remaining global scratch state)
- ⬜ Kernel spinlocks for real cross-CPU critical sections
  ("disable interrupts" alone stops working the moment a second core
  can run concurrently)
- ⬜ SMP-aware scheduler (per-CPU run queues or a shared one with locking)
- ⬜ 🎮 Multi-core game AI / physics!

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

- ⬜ ACPI shutdown / reboot
- ⬜ WiFi driver
- ⬜ Bluetooth driver
- ⬜ Suspend / resume
- ⬜ Multiple monitor support
- ⬜ Hardware detection / plug and play
- ⬜ User accounts and login
- ⬜ Enforce file permissions (uid/gid/mode already exist on every inode - currently unused/unenforced)
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
5th July 2026
            Phase 9 (ELF Loader) - SYSCALL/SYSRET, ring 3, exec()
            loading real ELF files, hello.c updated

11th July 2026
            Phase 9 complete - shell 'run' command loads and runs
            ELF binaries
            Phase 10 (Basic libc) mostly complete - acornlibc with
            crt0, syscall wrappers, printf, string functions, and
            fd-aware file I/O (open/read/write/close/seek working
            against real files). malloc/free and buffered stdio
            (fopen/fread/fwrite) still open

11th July 2026
            Roadmap comprehensiveness pass - reviewed the whole plan
            for foundational OS gaps and inserted new phases so
            nothing important gets skipped as we accelerate:
              Phase 7.5  - Real-Time Clock
              Phase 10.5 - Process & Program Fundamentals
              Phase 10.6 - Process Lifecycle, Signals & Safety
              Phase 11.5 - Kernel Robustness & Testing
              Phase 13.5 - Advanced Memory Management
              Phase 13.6 - Inter-Process Communication
              Phase 13.7 - Threads & Synchronization
              Phase 17.5 - FPU / SIMD Support & Math Library
              Phase 20.5 - Dynamic Linking & Shared Libraries
              Phase 27.5 - SMP / Multi-core Support
            Also added ACPI shutdown/reboot, user accounts/login, and
            file permission enforcement to Phase 29 (Daily Driveable).
            No renumbering of existing phases - new phases use decimal
            numbers and slot in between their neighbours.
```
