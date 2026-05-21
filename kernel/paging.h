#ifndef PAGING_H
#define PAGING_H

#include "idt.h"

#include <stdint.h>

// --- Page Directory Entry (PDE) -------------------------
// One entry in the Page Directory
// Each entry points to a Page Table
// The CPU reads these automatically when translating virtual addresses
typedef struct __attribute__((packed))
{
    uint32_t present            : 1;        // Bit 0 - 1 = this entry is valid
                                            //         0 = accessing this causes a page fault
    uint32_t writable           : 1;        // Bit 1 - 1 = page table is writable
                                            //         0 = read only
    uint32_t user               : 1;        // Bit 2 - 1 = user programs can access this
                                            //         0 = kernel only
    uint32_t write_through      : 1;        // Bit 3 - controls CPU cache write behaviour
    uint32_t cache_disabled     : 1;        // Bit 4 - 1 = disable CPU cache for this entry
    uint32_t accessed           : 1;        // Bit 5 - CPU sets this when entry is read
    uint32_t reserved           : 1;        // Bit 6 - must always be 0
    uint32_t page_size          : 1;        // Bit 7 - 0 = 4KB pages (what we use)
                                            //         1 = 4MB pages (we don't use this)
    uint32_t ignored            : 4;        // Bits 8-11 - ignored by CPU, we can use freely
    uint32_t frame              : 20;       // Bits 12-31 - upper 20 bits of the physical 
                                            //              address of the Page Table
                                            //              lower 12 bits are always 0
                                            //              (page tables are 4KB aligned)
} pde_t;

// --- Page Table Entry (PTE) -----------------------------------------
// One entry in a Page Table 
// Each entry points to a physical page of memory
typedef struct __attribute__((packed))
{
    uint32_t present            : 1;        // Bit 0 - 1 = this page exists in RAM
    uint32_t writable           : 1;        // Bit 1 - 1 = page is writable
    uint32_t user               : 1;        // Bit 2 - 1 = user programs can access
    uint32_t write_through      : 1;        // Bit 3 - cache write behaviour
    uint32_t cache_disabled     : 1;        // Bit 4 - disable cache for this page
    uint32_t accessed           : 1;        // Bit 5 - CPU sets when page is read
    uint32_t dirty              : 1;        // Bit 6 - CPU sets when page is written to
                                            //         useful for knowing which pages
                                            //         need writing back to disk
    uint32_t reserved           : 1;        // Bit 7 - must always be 0
    uint32_t global             : 1;        // Bit 8 - 1 = don't flush from TLB on
                                            //             CR3 reload (kernel pages use this)
    uint32_t ignored            : 3;        // Bits 9-11 - ignored by CPU
    uint32_t frame              : 20;       // Bits 12-31 - upper 20 bits of the physical
                                            //              page address this entry maps to.
} pte_t;

// --- Page Directory --------------------------------------------
// The top level structure - 1024 entries covering the full 4GB virtual space
// Each entry covers 4MB of virtual address space (1024 x 4KB)
// Must be 4KB aligned in memory so the CPU can find it via CR3
typedef struct __attribute__((aligned(4096)))
{
    pde_t entries[1024];                    // 1024 x 4 bytes = exactly 4KB
} page_directory_t;

// --- Page Table -------------------------------------------------
// Second level structure - 1024 entries covering 4MB of virtual space
// Each entry maps to one 4KB physical page
// Must also be 4KB aligned
typedef struct __attribute__((aligned(4096)))
{
    pte_t entries[1024];                    // 1024 x 4 bytes = exactly 4KB
} page_table_t;

// --- Flag constants ----------------------------------------------
// Used when setting up entries - OR these together for the flags you want
#define PAGE_PRESENT    0x1                 // Page is present in memory
#define PAGE_WRITABLE   0x2                 // Page is writable
#define PAGE_USER       0x3                 // Page is accessible from user space

void paging_init();                         // Set up page tables and enable paging
void page_fault_handler(registers_t* regs); // Called on interrupt 14

#endif