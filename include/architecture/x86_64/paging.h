#ifndef ARCH_PAGING_H
#define ARCH_PAGING_H

#include <kernel/paging.h>

#include <stdint.h>

// --- 64-bit Page Table Entry -------------------------------------
// Used for ALL levels: PML4, PDPT, PD, PT
// All entries are 8 bytes with the same basic format
typedef struct __attribute__((packed))
{
    uint64_t present                        : 1;    // Bit 0 - entry is valid
    uint64_t writable                       : 1;    // Bit 1 - writes allowed
    uint64_t user                           : 1;    // Bit 2 - user space accessible
    uint64_t write_through                  : 1;    // Bit 3 - cache write-through
    uint64_t cache_disabled                 : 1;    // Bit 4 - disable cache
    uint64_t accessed                       : 1;    // Bit 5 - CPU sets on access
    uint64_t dirty                          : 1;    // Bit 6 - CPU sets on write (PT only)
    uint64_t huge_page                      : 1;    // Bit 7 - 2MB page (PD) or 1GB (PDPT)
    uint64_t global                         : 1;    // Bit 8 - don't flush on CR3 reload
    uint64_t available                      : 3;    // Bits 9 - 11 - free for OS use
    uint64_t frame                          : 40;   // Bits 12 - 51 - physical address >> 12
    uint64_t available2                     : 11;   // Bits 52 - 62 - free for OS use
    uint64_t no_execute                     : 1;    // Bit 63 - NX bit (disable execution)
} page_entry_t;

// --- Page Table ------------------------------------------------------------
// 512 entries x 8 bytes = exactly 4KB
// Used for all levels: PML4, PDPT, PD, PT
typedef struct __attribute__((aligned(4096)))
{
    page_entry_t entries[512];
} page_table_t;

struct page_directory
{
    page_entry_t entries[512];
} __attribute__((aligned(4096)));

// --- Virtual address index macros ------------------------------------------

// Break a 64-bit virtual address into its component indicies
#define PML4_INDEX(addr)        (((uint64_t)(addr) >> 39) & 0x1FF)      // Bits 47 - 39
#define PDPT_INDEX(addr)        (((uint64_t)(addr) >> 30) & 0x1FF)      // Bits 38 - 30
#define PD_INDEX(addr)          (((uint64_t)(addr) >> 21) & 0x1FF)      // Bits 29 - 21
#define PT_INDEX(addr)          (((uint64_t)(addr) >> 12) & 0x1FF)      // Bits 20 - 12
#define PG_OFFSET(addr)         ((uint64_t)(addr) & 0xFFF)              // Bits 11 - 0

// --- Direct physical map base ---------------------------------------------
// All physcial RAM is mapped here by Stage 2 bootloader
// Physical address X -> virtual PHYSICAL_MAP_BASE + X
#define PHYSICAL_MAP_BASE       0xFFFF800000000000UL
#define PHYSICAL_TO_VIRTUAL(address) ((uint64_t)(address) + PHYSICAL_MAP_BASE)
#define VIRTUAL_TO_PHYSICAL(address) ((uint64_t)(address) - PHYSICAL_MAP_BASE)

// --- Kernel page directory -----------------------------------------------
// The kernel's PML4 - set up by Stage 2, extended by paging_init()
extern page_directory_t* kernel_pml4;

#endif