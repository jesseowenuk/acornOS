#include "paging.h"
#include "pmm.h"            // For PAGE_SIZE
#include "serial.h"         // For debug logging
#include "vga.h"            // For vga_print

// --- Page directory -----------------------------------
// We declare this statically so it lives in the kernel's BSS segment
// __attribute__((aligned(4096))) ensures it starts on a 4KB boundary
// which is required by the CPU - CR3 must point to a 4KB-aligned address
static page_directory_t kernel_directory __attribute__((aligned(4096)));

// --- Page table ----------------------------------------
// We need one page table for every 4MB of virtual space we want to map
// Each page table covers 1024 pages x 4KB = 4MB
// For now we'll identity map the first 4MB - enough for our kernel
static page_table_t first_table __attribute__((aligned(4096)));

// --- Helper: Set a page directory entry ------------------------
// Takes a PTE pointer, physical address of the page table and flags
static void pte_set(pte_t* entry, uint32_t phsical_addr, uint32_t flags)
{
    // Set present bit if PAGE_PRESENT flag given
    entry->present = (flags & PAGE_PRESENT) ? 1 : 0;

    // Set present bit if PAGE_WRITABLE flag given
    entry->writable = (flags & PAGE_WRITABLE) ? 1 : 0;

    // Set present bit if PAGE_USER flag given
    entry->user = (flags & PAGE_USER) ? 1 : 0;

    // Store upper 20 bits of physical address
    // >> 12 discards the lower 12 bits (always 0
    // since pages are 4KB aligned)
    entry->frame = phsical_addr >> 12;
}

// --- Helper: Set a page directory entry ------------------------
// Takes a PDE pointer, physical address of the page table and flags
static void pde_set(pde_t* entry, uint32_t table_phsical_addr, uint32_t flags)
{
    // Mark this directory entry as valid
    entry->present = (flags & PAGE_PRESENT) ? 1 : 0;

    // Allow writes through this directory entry
    entry->writable = (flags & PAGE_WRITABLE) ? 1 : 0;

    // Allow user access through this entry
    entry->user = (flags & PAGE_USER) ? 1 : 0;

    // Store physical address of the page table
    // Upper 20 bits only - same as PTE
    entry->frame = table_phsical_addr >> 12;
}

// --- Init --------------------------------------------------------

void paging_init()
{
    // Step 1: zero out the entire page directory
    // All entries start as not present - any access will page fault
    // until we explicitly map the pages we need.
    uint32_t* dir = (uint32_t*)&kernel_directory;
    for(int i = 0; i < 1024; i++)
    {
        // Clear all 1024 directory entries
        dir[i] = 0;                 
    }

    // Step 2: identity map the first 4MB of memory
    // This covers our bootloader, kernel, stack and heap
    // Virtual address X maps to physical address X
    // So all existing kernel code continues to work after paging is enabled
    for(int i = 0; i < 1024; i++)
    {
        // Page 0 = 0x0000, page 1 = 0x1000 etc.
        uint32_t physical_addr = i * PAGE_SIZE;

        pte_set(
            (pte_t*)&first_table.entries[i],            // Which PTE to set
            physical_addr,                      // Physical address to map to
            PAGE_PRESENT | PAGE_WRITABLE        // Present and writable
        );
    }

    // Step 3: install first_table into the page directory at index 0
    // Index 0 covers virtual addresses 0x00000000 - 0x003FFFFF (first 4MB)
    pde_set(
        (pde_t*)&kernel_directory.entries[0],           // First directory entry
        (uint32_t)&first_table,                 // Physical address of our page table
        PAGE_PRESENT | PAGE_WRITABLE            // Present and writable
    );

    serial_println("Paging: page directory created.");
    serial_print("Paging: kernel directory at 0x");

    // Print address in hex for verification
    uint32_t addr = (uint32_t)&kernel_directory;
    char hex[9];
    const char* digits = "0123456789ABCDEF";
    for(int i = 7; i >= 0; i--)
    {
        hex[i] = digits[addr & 0x0F];
        addr >>= 4;
    }

    hex[8] = 0;
    serial_println(hex);

    serial_println("Paging: first 4MB identity mapped.");

    // Step 4: load page directory address into CR3
    // CR3 is the Page Directory Base Register (PDBSR)
    // The CPU reads this to find our page directory on every TLB miss
    // we must pass the PHYSICAL address - paging isn't on yet so
    // virtual == physical at this point
    __asm__ volatile(
        "mov %0, %%cr3"                   // Load address of kernel_directory into CR3
        :                                // No output operands
        : "r"(&kernel_directory)         // Input: address of our page directory 
    );

    // Step 5: enable paging by setting bit 31 of CR0
    // we must read CR0 first, set the bit, then write it back
    // Bit 31 = PG (Paging) flag
    // The instant this executes, ALL memory accesses go through page tables
    uint32_t cr0;
    __asm__ volatile(
        "mov %%cr0, %0"                 // Read current CR0 value
        : "=r"(cr0)                     // Output: store in CR0 variale
    );
    cr0 |= 0x80000000;                  // Set bit 31 - the paging enable bit
                                        // 0x80000000 = 1000 0000 0000 0000 0000 0000 0000 0000

    __asm__ volatile(
        "mov %0, %%cr0"                 // Write modified CR0 back
        :                               // No output operands
        : "r"(cr0)                      // Input: our modified value
    );

    // If we reach here - paging is enabled and we survived!
    serial_println("Paging: enabled! Still alive.");
}