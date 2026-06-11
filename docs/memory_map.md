# acornOS 64-bit Memory Map
> **Definitive reference for all memory layout decisions.**
> Follows acornOS Engineering Principles — see docs/principles.md
> Last updated: June 2026

---

## Design Principles Applied Here

```
✅ Map ALL physical RAM — no artificial limits
✅ Load kernel of ANY size — no hardcoded sectors
✅ PMM bitmap sized dynamically from E820
✅ No magic numbers — all via named constants
✅ Reserved regions locked before first allocation
✅ Designed to work with any x86_64 machine
```

---

## Architecture

```
CPU mode:       64-bit long mode (x86_64)
Paging:         4-level (PML4 → PDPT → PD → PT)
Page size:      4KB standard
                2MB large pages (future — for direct map)
Kernel type:    Monolithic with driver isolation
Address split:  128TB user / 128TB kernel
```

---

## Constants

```c
//=============================================================
// Physical addresses — FIXED, hardware defined
//=============================================================

// BIOS puts boot drive here on entry
#define BOOT_DRIVE_ADDR             0x0000000000000600  // 1 byte

// E820 memory map (written by Stage 2)
#define E820_MAP_ADDR               0x0000000000000700  // up to 128 entries
#define E820_COUNT_ADDR             0x0000000000000500  // 4 bytes
#define E820_MAX_ENTRIES            128                 // more than enough

// Stage 1 bootloader (loaded by BIOS)
#define STAGE1_PHYS                 0x0000000000007C00  // fixed by BIOS spec

// Stage 2 bootloader (loaded by Stage 1)
#define STAGE2_PHYS                 0x0000000000007E00  // right after Stage 1
#define STAGE2_MAX_SIZE             (63 * 512)          // 63 sectors = 31.5KB

// Stage 2 stack (temporary, reclaimed after boot)
#define BOOT_STACK_PHYS             0x000000000001FFFF  // grows down from here
                                                        // to 0x10000 = 64KB stack

// VGA text buffer (hardware defined)
#define VGA_PHYS                    0x00000000000B8000  // always here on x86

//=============================================================
// Physical addresses — DYNAMIC, placed by us
//=============================================================

// Kernel loaded here by Stage 2
// Standard location, above 1MB, below BIOS hole
#define KERNEL_PHYS_BASE            0x0000000000100000  // 1MB mark

// Kernel sector count stored here by build system
// Stage 2 reads this to know how many sectors to load
// Stored in first sector of kernel on disk
#define KERNEL_SECTOR_COUNT_ADDR    0x00000000000FF000  // just below 1MB

// PMM bitmap — placed right after kernel
// Size calculated at boot from E820 map:
//   bitmap_size = total_pages / 8
//   total_pages = total_ram / PAGE_SIZE
// For 8GB:  8GB  / 4KB = 2M pages  / 8 = 256KB bitmap
// For 64GB: 64GB / 4KB = 16M pages / 8 = 2MB bitmap
// For 1TB:  1TB  / 4KB = 256M pages/ 8 = 32MB bitmap
// We reserve 32MB to support up to 1TB RAM
#define PMM_PHYS_BASE               0x0000000000200000  // 2MB mark
#define PMM_MAX_SIZE                (32 * 1024 * 1024)  // 32MB — supports 1TB RAM

// Kernel heap — placed right after PMM reservation
#define HEAP_PHYS_BASE              0x0000000002200000  // 34MB mark
#define HEAP_SIZE                   (16 * 1024 * 1024)  // 16MB

// Process physical pages allocated from here upward
// PMM manages this region dynamically
#define PROCESS_PHYS_BASE           0x0000000003200000  // 50MB mark
// No upper limit — PMM uses E820 to find all usable RAM above this

//=============================================================
// Virtual addresses — Kernel space
//=============================================================

// Direct physical map — ALL RAM mapped here
// Physical addr X → virtual PHYS_MAP_BASE + X
// Size: covers ALL detected RAM dynamically
// No artificial limit — if machine has 1TB we map 1TB
#define PHYS_MAP_BASE               0xFFFF800000000000

// vmalloc region — non-contiguous kernel allocations
#define VMALLOC_BASE                0xFFFFFF0000000000
#define VMALLOC_SIZE                (512ULL * 1024 * 1024 * 1024) // 512GB

// Kernel modules — loaded drivers
#define MODULES_BASE                0xFFFFFF8000000000
#define MODULES_SIZE                (2ULL * 1024 * 1024 * 1024)   // 2GB

// Kernel code and data
#define KERNEL_VIRT_BASE            0xFFFFFFFF80000000
#define KERNEL_ENTRY                0xFFFFFFFF80100000  // entry point

// Kernel heap virtual address
// Maps to HEAP_PHYS_BASE physically
#define HEAP_VIRT_BASE              0xFFFFFFFF82200000

// PMM bitmap virtual address
// Maps to PMM_PHYS_BASE physically
#define PMM_VIRT_BASE               0xFFFFFFFF80200000

// VGA buffer virtual address
// Maps to VGA_PHYS physically
#define VGA_VIRT_BASE               0xFFFFFFFF800B8000

// Per-CPU kernel stacks
// Each CPU gets a 64KB stack
#define KERNEL_STACK_BASE           0xFFFFFFFF90000000
#define KERNEL_STACK_SIZE           (64 * 1024)         // 64KB per CPU
#define MAX_CPUS                    256                 // support up to 256 cores

// Fixed kernel mappings — never change, always present
#define FIXED_MAP_BASE              0xFFFFFFFFFFFF0000
#define FIXED_MAP_SIZE              (64 * 1024)         // 64KB

//=============================================================
// Virtual addresses — User space
//=============================================================

// NULL guard — never mapped
#define USER_NULL_GUARD             0x0000000000000000
#define USER_NULL_GUARD_SIZE        (4 * 1024)          // 4KB

// User space start
#define USER_SPACE_START            0x0000000000001000

// ELF executables load here
#define USER_ELF_BASE               0x0000000000400000

// User stack — grows downward from top of user space
#define USER_STACK_TOP              0x00007FFFFFFFFFFF
#define USER_STACK_MAX              (8 * 1024 * 1024)   // 8MB max stack

// End of user space (non-canonical gap begins)
#define USER_SPACE_END              0x00007FFFFFFFFFFF

//=============================================================
// Address space conversions
//=============================================================

#define PHYS_TO_VIRT(addr)  ((uint64_t)(addr) + PHYS_MAP_BASE)
#define VIRT_TO_PHYS(addr)  ((uint64_t)(addr) - PHYS_MAP_BASE)

//=============================================================
// Page sizes
//=============================================================

#define PAGE_SIZE                   4096                    // 4KB standard page
#define LARGE_PAGE_SIZE             (2 * 1024 * 1024)       // 2MB large page
#define HUGE_PAGE_SIZE              (1024 * 1024 * 1024)    // 1GB huge page

//=============================================================
// Limits
//=============================================================

#define MAX_PROCESSES               64
#define KERNEL_STACK_PER_PROC       (4 * 1024)          // 4KB per process
#define MAX_RAM_SUPPORTED           (1ULL * 1024 * 1024 * 1024 * 1024) // 1TB
```

---

## Physical Memory Layout

```
Physical Address            Size            Contents
────────────────────────────────────────────────────────────────────
0x0000000000000000          1KB             Real mode IVT
                                            DO NOT USE
0x0000000000000400          256B            BIOS data area
                                            DO NOT USE
0x0000000000000500          4B              E820 entry count
                                            Written by Stage 2
0x0000000000000700          up to 3KB       E820 memory map
                                            Written by Stage 2
                                            128 entries x 24 bytes
0x0000000000001000          26KB            Free low memory
                                            Available for future use
0x0000000000007C00          512B            Stage 1 bootloader
                                            Loaded by BIOS
0x0000000000007E00          31.5KB          Stage 2 bootloader
                                            Loaded by Stage 1
                                            Up to 63 sectors
0x0000000000010000          64KB            Boot stack
                                            Stage 2 only
                                            Reclaimed after boot
0x0000000000020000          ~488KB          Free low memory
0x000000000009F000          4KB             EBDA
                                            DO NOT USE
0x00000000000A0000          384KB           BIOS/VGA reserved
                                            DO NOT USE
0x00000000000B8000          4KB             VGA text buffer
0x00000000000C0000          256KB           BIOS extensions
                                            DO NOT USE
0x00000000000F0000          64KB            BIOS ROM
                                            DO NOT USE
0x00000000000FF000          4KB             Kernel sector count
                                            Written by Makefile
                                            Read by Stage 2
0x0000000000100000          ~2MB            Kernel binary
                                            Loaded by Stage 2
                                            Exact size varies
                                            Stage 2 reads count
                                            from 0xFF000
0x0000000000200000          32MB            PMM bitmap
                                            Dynamically sized
                                            at boot from E820
                                            Supports up to 1TB RAM
0x0000000002200000          16MB            Kernel heap
                                            Managed by kmalloc/kfree
0x0000000003200000          varies          Process physical pages
                                            PMM allocates on demand
                                            From E820 usable regions
                                            No upper limit
...                         ...             All remaining RAM
                                            managed by PMM
```

---

## Virtual Memory Layout

### User Space (per process, 128TB)

```
Virtual Address             Size            Contents
────────────────────────────────────────────────────────────────────
0x0000000000000000          4KB             NULL guard
                                            Never mapped
                                            Catches null ptr deref
0x0000000000001000          ~4MB            Low user mappings
                                            (rarely used)
0x0000000000400000          varies          ELF .text
                                            Executable code
0x0000000000600000          varies          ELF .data / .bss
                                            Initialised data
0x0000000001000000          grows up        Process heap
                                            Grows upward via brk()
...                         ...             Free virtual space
                                            (enormous - 128TB!)
0x00007FFFF7000000          8MB             Shared libraries
                                            (future)
0x00007FFFFFFFE000          8MB             User stack
                                            Grows downward
                                            Hard limit: 8MB
0x00007FFFFFFFFFFF          ---             End of user space
```

### Non-Canonical Gap (hardware enforced)

```
0x0000800000000000          ~16EB           NON-CANONICAL HOLE
...                                         Hardware enforced
0xFFFF7FFFFFFFFFFF                          Any access = #GP fault
                                            Cannot be mapped
```

### Kernel Space (shared across ALL processes, 128TB)

```
Virtual Address             Size            Contents
────────────────────────────────────────────────────────────────────
0xFFFF800000000000          dynamic         DIRECT PHYSICAL MAP
                                            ALL physical RAM here
                                            Size = total RAM detected
                                            by E820 at boot
                                            No artificial limit

                                            Usage:
                                            ptr = PHYS_TO_VIRT(phys)
                                            Kernel accesses any
                                            physical address this way

                                            Example (8GB machine):
                                            0xFFFF800000000000 -> 0x0
                                            0xFFFF800000001000 -> 0x1000
                                            0xFFFF800200000000 -> end of RAM

0xFFFFFF0000000000          512GB           VMALLOC REGION
                                            Non-contiguous kernel
                                            virtual allocations
                                            Used when physical
                                            contiguity not needed

0xFFFFFF8000000000          2GB             KERNEL MODULES
                                            Loaded drivers
                                            Each gets own region
                                            Isolated from each other

0xFFFFFFFF80000000          2GB             KERNEL CODE AND DATA
                                            +---------------------+
                                            | 0xFFFFFFFF800B8000  |
                                            | VGA text buffer     |
                                            | (4KB)               |
                                            +---------------------+
                                            | 0xFFFFFFFF80100000  |
                                            | Kernel entry point  |
                                            | .text section       |
                                            | (kernel code)       |
                                            +---------------------+
                                            | 0xFFFFFFFF80200000  |
                                            | PMM bitmap          |
                                            | (up to 32MB)        |
                                            | Dynamic size        |
                                            +---------------------+
                                            | 0xFFFFFFFF82200000  |
                                            | Kernel heap         |
                                            | (16MB fixed)        |
                                            | kmalloc/kfree       |
                                            +---------------------+
                                            | 0xFFFFFFFF83200000  |
                                            | .rodata section     |
                                            +---------------------+
                                            | 0xFFFFFFFF84000000  |
                                            | .data section       |
                                            +---------------------+
                                            | 0xFFFFFFFF84800000  |
                                            | .bss section        |
                                            +---------------------+

0xFFFFFFFF90000000          16MB            KERNEL STACKS
                                            Per-CPU stacks
                                            64KB per CPU
                                            Supports 256 CPUs
                                            (future SMP support)

0xFFFFFFFFFFFF0000          64KB            FIXED MAPPINGS
                                            Always present
                                            Never change
                                            Emergency/debug use
```

---

## 4-Level Paging Structure

```
CR3 -> PML4 table (4KB, 512 entries of 8 bytes)
        |
        +-- Entry 0   -> PDPT for user space
        |               (processes fill this in)
        |
        +-- Entry 256 -> PDPT for direct physical map
        |               (0xFFFF800000000000)
        |               Covers ALL physical RAM
        |               Built at boot, never changes
        |
        +-- Entry 511 -> PDPT for kernel space
                        (0xFFFFFFFF80000000)
                        Shared across all processes
                        Built at boot, never changes

Virtual address bit breakdown:
+----------+----------+----------+----------+------------+
|  PML4    |  PDPT    |    PD    |    PT    |   Offset   |
| [47:39]  | [38:30]  | [29:21]  | [20:12]  |  [11:0]    |
|  9 bits  |  9 bits  |  9 bits  |  9 bits  |  12 bits   |
| 512 idx  | 512 idx  | 512 idx  | 512 idx  |  4KB page  |
+----------+----------+----------+----------+------------+

Key address breakdowns:

Kernel entry 0xFFFFFFFF80100000:
  PML4:   511  (0x1FF)  <- kernel PML4 entry
  PDPT:   510  (0x1FE)
  PD:     0    (0x000)
  PT:     256  (0x100)
  Offset: 0

Direct map 0xFFFF800000000000:
  PML4:   256  (0x100)  <- direct map PML4 entry
  PDPT:   0
  PD:     0
  PT:     0
  Offset: 0

User ELF 0x0000000000400000:
  PML4:   0    (0x000)  <- user PML4 entry
  PDPT:   0
  PD:     2    (0x002)
  PT:     0
  Offset: 0
```

---

## Boot Sequence (Detailed)

```
=== Stage 1 (real mode, 512 bytes) ===

1.  Save boot drive (DL) to 0x600
2.  Set up real mode segments
3.  LBA read Stage 2:
    - Sectors 1-63 (31.5KB)
    - Load to 0x7E00
4.  Jump to 0x7E00

=== Stage 2 (real mode -> long mode) ===

5.  Set up stack at 0x1FFFF (grows down)
6.  Enable A20 line:
    - Try BIOS method (INT 0x15 AX=0x2401)
    - Try Fast A20 (port 0x92)
    - Verify A20 is enabled
    - Panic if A20 fails
7.  E820 memory detection:
    - Store entries at 0x700
    - Store count at 0x500
    - Find total RAM size
    - Find highest usable address
8.  Read kernel sector count from disk:
    - Written by Makefile into sector at 0xFF000
    - Tells us exactly how many sectors to load
    - Load ALL of them — no guessing
9.  Load kernel to 0x100000:
    - LBA reads from sector 64 onwards
    - Load exactly kernel_sector_count sectors
    - Verify load succeeded
10. Set up temporary GDT (32-bit descriptors)
11. Enter protected mode (32-bit)
12. Set up 64-bit page tables:
    a. Allocate PML4, PDPT, PD, PT tables
       from free memory above Stage 2
    b. Identity map first 2MB (temporary)
       so we can keep executing
    c. Map kernel:
       0xFFFFFFFF80100000 -> 0x100000
       (enough to cover kernel size)
    d. Map direct physical map:
       0xFFFF800000000000 -> 0x0
       Size: cover all RAM found by E820
       Use 2MB large pages for efficiency
    e. Map VGA buffer:
       0xFFFFFFFF800B8000 -> 0xB8000
13. Enable PAE (CR4.PAE = 1)
14. Load PML4 into CR3
15. Enable long mode (EFER.LME = 1)
16. Enable paging (CR0.PG = 1)
    (CPU now in compatibility mode)
17. Far jump with 64-bit code segment
    (CPU now in full 64-bit long mode)
18. Set up 64-bit stack
19. Pass to kernel:
    - mem_map_addr  (E820 map address)
    - mem_map_count (E820 entry count)
    - total_ram     (bytes, from E820)
20. Jump to 0xFFFFFFFF80100000

=== Kernel entry (start.asm, 64-bit) ===

21. Set up kernel stack
22. Zero BSS section
23. Extend direct physical map:
    - Stage 2 mapped first few GB
    - Kernel extends to cover ALL RAM
    - Uses 2MB large pages
24. Remove temporary identity map
    (first 2MB no longer identity mapped)
25. Set up proper GDT (64-bit)
26. Set up IDT (64-bit)
27. Call kernel_main(mem_map_addr,
                     mem_map_count,
                     total_ram)

=== kernel_main ===

28. Normal boot continues...
```

---

## Disk Layout

```
Sector          Size        Contents
──────────────────────────────────────────────────────
0               512B        Stage 1 (MBR)
                            Must end with 0xAA55
1-63            31.5KB      Stage 2
64+             varies      Kernel binary
                            Stage 2 reads sector count
                            from disk metadata
                            Loads ALL sectors
                            No hardcoded limit ever
```

### How Dynamic Kernel Loading Works

```
At build time (Makefile):
1. Compile kernel -> kernel.bin
2. Calculate sector count:
   sectors = ceil(kernel_size / 512)
3. Write sector count to metadata
   sector at disk offset 0xFF000
4. Write kernel to disk at sector 64

At boot time (Stage 2):
1. Read metadata sector from 0xFF000
2. Extract sector count
3. Load exactly that many sectors
4. Done - works for kernel of ANY size
5. Never need to touch bootloader again
```

---

## PMM Dynamic Sizing

```
At boot time Stage 2 runs E820 to find total RAM.
Kernel then sizes PMM bitmap dynamically:

uint64_t total_ram   = e820_get_total_usable();
uint64_t total_pages = total_ram / PAGE_SIZE;
uint64_t bitmap_size = total_pages / 8;

Examples:
  8GB  RAM -> 2M pages   -> 256KB bitmap  (fits in 32MB reservation)
  64GB RAM -> 16M pages  -> 2MB bitmap    (fits in 32MB reservation)
  512GB RAM -> 128M pages -> 16MB bitmap  (fits in 32MB reservation)
  1TB  RAM -> 256M pages -> 32MB bitmap   (exactly fits - our max)
```

---

## Memory Regions Reserved by PMM at Boot

```
Before PMM allocates a single page it marks
ALL of these as used:

Physical region             Reason
────────────────────────────────────────────────────
0x000000 - 0x000FFF        Real mode IVT/BIOS
0x007C00 - 0x007DFF        Stage 1 bootloader
0x007E00 - 0x00FFFF        Stage 2 + boot stack
0x09F000 - 0x09FFFF        EBDA
0x0A0000 - 0x0FFFFF        BIOS/VGA/ROM reserved
0x100000 - 0x1FFFFF        Kernel binary
0x200000 - 0x21FFFF        PMM bitmap (dynamic size)
0x220000 - 0x31FFFF        Kernel heap (16MB)

Then PMM marks E820 usable regions as FREE
MINUS the reserved regions above

Result: PMM never allocates over anything important
```

---

## Process Memory Layout

```
Each process gets:
- Its own PML4 table (shares kernel entries)
- Its own user space mappings
- A kernel stack (4KB, from PMM)
- A user stack (grows from USER_STACK_TOP down)

Kernel entries shared across all processes:
- PML4[256] -> direct physical map
- PML4[511] -> kernel code/data/heap

User entries unique per process:
- PML4[0] -> process's own user space
```

---

## Future Considerations

```
SMP (multiple cores):
  Each CPU gets its own kernel stack
  KERNEL_STACK_BASE + (cpu_id * KERNEL_STACK_SIZE)
  Max 256 CPUs supported

Large pages (2MB):
  Direct physical map uses 2MB pages
  Much more efficient for large RAM
  Fewer TLB entries needed

Huge pages (1GB):
  For very large RAM machines
  Even fewer TLB entries
  Future optimisation

5-level paging (future):
  CR4.LA57 = 1
  Extends to 57-bit virtual addresses
  128PB virtual space
  When we need more than 128TB

NUMA (Non-Uniform Memory Access):
  Multiple memory controllers
  PMM tracks which NUMA node each page is on
  Allocate from local node for performance
  Future optimisation for multi-socket systems

ARM port:
  Same virtual layout
  Different physical setup
  Separate memory_map_arm64.md
```

---

## Change Log

```
June 2026   Initial 64-bit memory map
            Principles applied:
            - Dynamic RAM detection (no limits)
            - Dynamic kernel loading (no hardcoding)
            - Dynamic PMM bitmap sizing
            - Full reservation before allocation
            - Supports up to 1TB RAM
            - Supports up to 256 CPUs (future)
```
