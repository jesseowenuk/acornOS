# acornOS — Claude Code Handoff Document

## ⚠️ IMPORTANT: READ BEFORE DOING ANYTHING

**DO NOT edit any code directly.** Always show the human the exact changes needed and let them make the edits. The human has strong preferences about this workflow — they make all edits, you provide the guidance.

**Always specify the exact file and code landmark** (surrounding lines) when showing where a change goes.

**Compile as you go** — after each change, rebuild and check for errors before moving on.

**One change at a time** — do not dump large blocks of changes at once.

---

## Project Overview

**acornOS** — A custom x86_64 operating system written in C++20/C/Assembly, built from scratch on macOS (Apple Silicon M1). Long-term goal: produce a feature-length animated film using the OS.

- **GitHub:** jesseowenuk/acornOS
- **Remote:** `AcornOS` (not `origin`)
- **Current branch:** `main`

---

## Engineering Principles

1. **Do it properly first time** — no hacks, no artificial limits
2. **No hardcoded values** — everything dynamic
3. **One change at a time** — compile and verify at each step
4. **Always specify file and code landmark** when showing changes
5. **Alphabetical includes** in all files
6. **Wrap all inline assembly in functions** — never raw asm in business logic
7. **Keep debug prints** — they stay until we're 100% confident

---

## Build System

```bash
make clean && make run    # Always use this — never just make
```

**Toolchain:**
- Compiler: `x86_64-elf-gcc`
- Linker: `x86_64-elf-ld`
- Assembler: `nasm`
- Emulator: `qemu-system-x86_64`

**QEMU flags:**
```makefile
qemu-system-x86_64 \
    -drive file=build/os.img,format=raw,index=0,media=disk \
    -serial stdio \
    -m 256M \
    -cpu qemu64,+rdrand \
    -no-reboot
```

**Build output:** All artifacts in `build/`

**Disk layout:**
```
Sector 0:        Stage 1 bootloader
Sectors 1-63:    Stage 2 bootloader
Sector 63:       Kernel size metadata
Sectors 64+:     Kernel binary
Sector 512:      hello.elf (first user space program)
```

---

## Directory Structure

```
acornOS/
├── boot/                          # Bootloader
│   ├── bootsect.asm               # Stage 1 (512 bytes)
│   └── stage2.asm                 # Stage 2 (real→protected→long mode)
│
├── kernel/
│   ├── architecture/x86_64/       # x86_64 specific implementation
│   │   ├── gdt.c / gdt_flush.asm
│   │   ├── idt.c / idt_flush.asm
│   │   ├── isr.asm
│   │   ├── paging.c               # 4-level page tables
│   │   ├── pic.c
│   │   ├── start.asm              # Kernel entry point
│   │   ├── switch.asm             # Context switch — CURRENT FOCUS
│   │   ├── syscall_entry.asm      # SYSCALL/SYSRET entry
│   │   ├── tss.c
│   │   ├── usermode.asm           # enter_usermode / iret_to_usermode
│   │   └── usermode.c
│   ├── core/
│   │   ├── elf.c                  # ELF loader — CURRENT FOCUS
│   │   ├── kernel.c               # kernel_main
│   │   ├── kprintf.c              # Supports %lx, %lu, %ld
│   │   └── panic.c
│   ├── memory/
│   │   ├── mem.c                  # Heap allocator
│   │   └── pmm.c                  # Physical memory manager
│   └── processes/
│       ├── process.c              # Process management — CURRENT FOCUS
│       ├── scheduler.c            # Round-robin scheduler — CURRENT FOCUS
│       └── syscall.c              # Syscall handlers
│
├── drivers/
│   ├── display/vga.c
│   ├── input/keyboard.c
│   ├── null/null.c
│   ├── random/random.c            # Uses RDRAND with CPUID check
│   ├── serial/serial.c
│   └── timer/timer.c
│
├── file_system/
│   ├── devfs/devfs.c              # Device filesystem (/devices)
│   ├── procfs/procfs.c            # Process filesystem (/process)
│   ├── shadowfs/shadowfs.c        # RAM filesystem (/temp)
│   └── vfs/vfs.c                  # Virtual filesystem layer
│
├── apps/
│   ├── hello/hello.c              # First user space program — CURRENT FOCUS
│   └── shell/shell.c              # Kernel shell
│
├── include/
│   ├── architecture/x86_64/       # x86_64 private headers
│   │   ├── gdt.h
│   │   ├── idt.h
│   │   ├── paging.h               # page_entry_t, page_table_t etc
│   │   ├── pic.h
│   │   ├── tss.h
│   │   └── usermode.h
│   ├── drivers/                   # Driver headers
│   ├── file_system/               # Filesystem headers
│   └── kernel/                    # Generic kernel headers
│       ├── elf.h
│       ├── gdt.h                  # Generic interface
│       ├── interrupts.h           # registers_t, idt_init, syscall_msr_init
│       ├── paging.h               # Generic paging interface
│       ├── pic.h
│       ├── tss.h
│       └── processes/
│           ├── process.h
│           ├── scheduler.h
│           └── syscall.h
│
├── docs/
│   ├── memory_map.md
│   ├── principles.md
│   └── roadmap.md
│
└── tools/
    └── write_kernel_size.sh
```

---

## Memory Map

```
Physical:
0x0000000000000000   Null page (reserved)
0x0000000000001000   Stage 2 page tables (PML4 etc)
0x0000000000007E00   Stage 2 bootloader
0x000000000000A000   Boot scratch (highest_ram, kernel_sectors)
0x0000000000020000   Kernel reserved region ends
0x0000000000021000   PMM allocations start here
0x0000000000100000   Kernel binary (1MB mark)
0x0000000000200000   PMM bitmap
0x0000000002200000   Kernel heap (16MB)
0x0000000003000000   hello.elf loaded by Stage 2 (TEMPORARY)

Virtual:
0x0000000000400000   User space ELF programs (.text)
0x00007FFFFFFFE000   User stack (grows down)
0xFFFF800000000000   Direct physical map base (PHYS_MAP_BASE)
0xFFFF800000001000   Kernel PML4 (via direct map)
0xFFFF800000200000   PMM bitmap (via direct map)
0xFFFF800002200000   Kernel heap (via direct map)
0xFFFFFFFF80000000   Kernel space start
0xFFFFFFFF80100000   Kernel entry point
0xFFFFFFFF800B8000   VGA buffer
```

---

## Key Constants

```c
PHYS_MAP_BASE       = 0xFFFF800000000000UL
KERNEL_ENTRY        = 0xFFFFFFFF80100000
PMM_BITMAP_ADDRESS  = 0xFFFF800000200000UL
HEAP_START          = 0xFFFF800002200000UL
HEAP_SIZE           = 16MB
VGA_MEMORY          = 0xFFFFFFFF800B8000UL
HELLO_ELF_PHYS      = 0x300000            // Temporary
USER_STACK_TOP      = 0x00007FFFFFFFE000UL
```

---

## process_t Struct Offsets (used in switch.asm)

```
pid          = 0   (8 bytes)
name         = 8   (32 bytes)
state        = 40  (4 bytes)
padding      = 44  (4 bytes)
cpu          = 48  starts here:
  rax        = 48
  rbx        = 56
  rcx        = 64
  rdx        = 72
  rsi        = 80
  rdi        = 88
  rbp        = 96
  rsp        = 104
  rip        = 112
  rflags     = 120
  cs         = 128
  ds         = 136
  ss         = 144
stack        = 152
stack_top    = 160
page_dir     = 168
```

---

## Current Status — What Works ✅

- **Bootloader:** Two-stage, loads kernel + hello.elf from disk
- **Long mode:** 64-bit kernel running at 0xFFFFFFFF80100000
- **Page tables:** 4-level paging, direct physical map
- **GDT/IDT/TSS:** 64-bit versions working
- **PIC/Timer/Keyboard:** All working
- **Memory:** Heap + PMM with E820 detection
- **Processes:** Creation, scheduling (round-robin), exit
- **Scheduler:** Timer-driven preemption working for kernel processes
- **Syscalls:** INT 0x80 working for kernel processes, SYSCALL/SYSRET set up but not yet tested
- **VFS:** Working with shadowFS, devFS, procFS
- **shadowFS:** Full implementation (read/write/readdir/mkdir/delete/truncate)
- **devFS:** /devices/display, keyboard, null, random, serial
- **procFS:** /process/meminfo, /process/[pid]/status etc
- **Shell:** Running with ls/cat/mkdir/rm/ps/mem commands
- **ELF loader:** Loads hello.elf from physical 0x300000, maps segments, sets up user stack

---

## Current Problem — What We're Trying To Fix 🔧

**Goal:** Run hello.elf in ring 3 (user space)

**hello.elf** is a simple program:
```c
static const char msg[] = "Hello from user space!\n";
void _start() {
    // SYS_WRITE via INT 0x80
    // SYS_EXIT via INT 0x80
}
```

**Current symptom:**
```
PAGE FAULT: CR3=0x0000000000024000  ← hello's PML4
PAGE FAULT at 0x0000000000000038
Error: present=0 write=0 user=0
RIP: 0xFFFFFFFF80100214
```

Or sometimes triple fault immediately after scheduler starts.

**Root cause analysis so far:**

1. `switch.asm` ring 3 restore path (`.ring3_restore`) is broken
2. The iretq frame setup is inconsistent between `elf_load` and `switch_context`
3. `scheduler_start` inline assembly may be corrupting the stack

**What elf_load currently does:**
- Loads ELF segments into hello's page directory
- Maps user stack at 0x7FFFFFFFE000
- Sets up iretq frame on hello's kernel stack:
  ```
  [stack_top]     = SS (0x23)
  [stack_top-8]   = RSP (user stack top)
  [stack_top-16]  = RFLAGS (0x200)
  [stack_top-24]  = CS (0x1B)
  [stack_top-32]  = RIP (0x400000, entry point)
  ```
- Sets `cpu.rsp` to point at top of iretq frame
- Sets `cpu.cs = 0x1B` so switch_context takes ring3 path
- Sets `cpu.rip` (not used in ring3 path)

**What switch_context ring3 path does:**
- Detects ring3 via `cs & 3 != 0`
- Loads user data segments (0x23)
- Saves RSI to RAX temporarily
- Restores general registers from PCB
- Loads RSP from cpu.rsp (iretq frame)
- Executes iretq

**Known issues:**
- `scheduler_start` does `ret` to jump to first process (idle)
  - idle's entry is pushed onto stack_top during process_create
  - BUT something is corrupting this value
- The `*stack_top` shows `kernel_main` address instead of `idle_process`
  - Suggesting stack corruption during process/ELF setup

---

## GDT Selectors

```
0x00  Null
0x08  Kernel code (ring 0, 64-bit, L=1)
0x10  Kernel data (ring 0)
0x18  User code base (without RPL)
0x1B  User code (ring 3, = 0x18 | 3)
0x20  User data base (without RPL)
0x23  User data (ring 3, = 0x20 | 3)
0x28  TSS (64-bit, 16 bytes = entries 5+6)
```

---

## Syscall Numbers (INT 0x80 convention)

```
0  SYS_EXIT
1  SYS_WRITE    (rbx=str, rcx=len)
2  SYS_READ
3  SYS_GETPID
4  SYS_YIELD
5  SYS_FORK
6  SYS_WAIT
7  SYS_EXEC
8  SYS_OPEN
9  SYS_CLOSE
10 SYS_SEEK
11 SYS_MKDIR
12 SYS_READDIR
13 SYS_DELETE
```

**NOTE:** INT 0x80 is temporary. Will switch to SYSCALL instruction when proper ring 3 is working. Argument convention will change to rdi/rsi/rdx.

---

## Immediate Next Steps

### Step 1: Fix scheduler_start stack corruption
Verify what value is actually at idle's `stack_top` just before the `ret`.
The human suspects `*stack_top` contains `kernel_main` instead of `idle_process`.

### Step 2: Fix ring3 context switch
Once idle works reliably, ensure the timer-driven switch to hello uses the
correct iretq path in `switch.asm`.

### Step 3: Verify hello runs in ring 3
`Hello from user space!` should appear on screen/serial.

### Step 4: Switch hello.c to SYSCALL instruction
Replace INT 0x80 with proper 64-bit SYSCALL convention.

---

## Recent Changes That May Be Relevant

1. `switch.asm` ring0 restore: `mov rax → mov rsp` (fixed RSP not being loaded)
2. `elf_load`: Sets `cpu.cs = 0x1B` to trigger ring3 path in switch_context
3. `scheduler_start`: Removed `sti`, removed `popfq`, now just `mov rsp; ret`
4. `idle_process`: Added `sti` inside the function before `hlt`
5. `process_create`: Handles `PROCESS_USER` flag (sets ring3 selectors)
6. Intermediate page table entries: user bit deliberately NOT set (kernel only)
7. `map_page` / `map_page_in`: Fixed `~0xFF` → `~0xFFF` alignment mask

---

## How To Debug

**Serial output** is the primary debug channel:
```bash
make clean && make run 2>/dev/null
```
Serial goes to stdout.

**For assembly-level issues**, look up RIP in the kernel map:
```bash
grep "XXXXXXX" build/kernel.map | head -5
```

**For disassembly:**
Physical offset = virtual - 0xFFFFFFFF80100000
```bash
x86_64-elf-objdump -b binary -m i386:x86-64 -D build/kernel.bin | grep -B5 " OFFSET:"
```

**Important:** GDB doesn't work on Mac. No telnet monitor (M1 QEMU limitation).
Use serial printf debugging only.

---

## Human Preferences

- **Never dump large code blocks** — one section at a time
- **Always specify file AND code landmark** for every change
- **Compile after each change** — don't batch changes
- **Ask before restructuring** anything significant
- **Keep debug prints** — they stay until the human removes them
- **Alphabetical includes** in all source files
- **Wrap inline assembly** in named functions always
- **Per our principles** — do it properly, no hacks

