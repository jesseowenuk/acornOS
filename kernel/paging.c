#include "paging.h"
#include "pmm.h"            // For PAGE_SIZE
#include "serial.h"         // For debug logging
#include "vga.h"            // For vga_print
#include "kprintf.h"

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

    kprintf("Paging: page directory created.\n");
    kserial_printf("Paging: kernel directory at 0x%x\n", (uint32_t)&kernel_directory);
    kserial_printf("Paging: first 4MB identity mapped.\n");

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
    kserial_printf("Paging: enabled! Still alive.\n");
}

// --- Page fault handler ---------------------------------------
// Called by isr_handler in idt.c when interrupt 14 fires
// regs->err_code contains the fault flags
// CR2 contains the virtual address that caused the fault
void page_fault_handler(registers_t* regs)
{
    // Read CR2 - the CPU stores the faulting address here automatically
    uint32_t faulting_addr;
    __asm__ volatile(
        "mov %%cr2, %0"             // Read CR2 into faulting_addr
        : "=r"(faulting_addr)       // Output operand
    );

    // Decode the error code bits
    // Present
    // 0 = page not present
    // 1 = page present but protection violated
    int present = regs->err_code & 0x1;

    // Write
    // 0 = fault happened on a read
    // 1 = fault happened on a write
    int write = regs->err_code & 0x2;

    // User
    // 0 = kernel was accessing the address
    // 1 = user program was accessing it
    int user = regs->err_code & 0x4;

    // Print fault information to VGA
    vga_set_colour(RED, BLACK);
    kprintf("\n--- PAGE FAULT ---\n");
    vga_set_colour(WHITE, BLACK);

    kprintf("Address : 0x%x\n", faulting_addr);
    kprintf("Reason  : %s %s %s\n", 
        present ? "protection violation" : "page not present",
        write   ? "on write"             : "on read",
        user    ? "from user space"      : "from kernel");
    kserial_printf("PAGE FAULT at 0x%x\n", faulting_addr);

    // Hang - we can't safely continue after a page fault
    for(;;);
}

// --- Page table storage ------------------------------------

// Allocate a fresh zeroed page table from our pool
static page_table_t* alloc_table()
{
    // PMM gives us a free 4KB physical page - exactly the right size
    page_table_t* table = (page_table_t*)pmm_alloc();
    
    if(!table)
    {
        kserial_printf("alloc_table: PMM out of memory!\n");
        return 0;
    }

    // Zero out every entry - all pages start out as not present
    uint32_t* t = (uint32_t*)table;
    for(int j = 0; j < 1024; j++)
    {
        // Not present, not writable
        t[j] = 0;
    }

    return table;
}

// --- map_page --------------------------------------------
// Maps a single virtual address to a physical address with fiven flags
// Both addresses are rounded down to 4KB page boundaries automatically
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    // Round both addresses down to page boundaries
    // e.g. 0x1234 becomes 0x1000 - we always map whole pages
    // Clear bottom 12 bits
    virtual_addr &= ~0xFFF;
    physical_addr &= ~0xFFF;

    // Get the page directory index from the virtual address
    // Which entry in the page directory?
    uint32_t pdi = PD_INDEX(virtual_addr);

    // Get the page table index from the virtual address
    // Which entry in the page table?
    uint32_t pti = PT_INDEX(virtual_addr);

    // Check if a page table already exists in this direcrory entry
    if(!kernel_directory.entries[pdi].present)
    {
        // No page table yet so lets create one
        page_table_t* new_table = alloc_table();

        if(!new_table)
        {
            kserial_printf("map page: out of page tables!\n");
            return;
        }

        // Install the new table into the directory
        pde_set(
            (pde_t*)&kernel_directory.entries[pdi],
            (uint32_t)new_table,                // physical address of the new table
            PAGE_PRESENT | PAGE_WRITABLE
        );
    }

    // Get pointer to the page table for this directory entry
    // frame contains the upper 20 bits of the table address
    // shift left 12 to get the full address back
    page_table_t* table = (page_table_t*)(
        kernel_directory.entries[pdi].frame << 12
    );

    // Set the page table entry to map virtual -> physical
    pte_set(
        (pte_t*)&table->entries[pti],           // Which PTE to set
        physical_addr,                          // Physical address to map to
        flags                                   // Present, writable, user etc.
    );

    // Flush the TLB entry for this virtual address
    // The CPU caches translations in the TLB - we must invalidate
    // the old entry or the CPU will use the stale cached translation
    __asm__ volatile(
        "invlpg (%0)"                           // Invalidate TLB entry for this address
        :                                       // No output
        : "r"(virtual_addr)                     // Input: the virtual address to flush
        : "memory"                              // Tells compiler memory may have changed
    );
}

// --- unmap_page ----------------------------------------------------
// Remove a virtual address mapping
void unmap_page(uint32_t virtual_addr)
{
    virtual_addr &= ~0xFFF;                 // Round to page boundary

    uint32_t pdi = PD_INDEX(virtual_addr);
    uint32_t pti = PT_INDEX(virtual_addr);

    // Check directory entry exists
    if(!kernel_directory.entries[pdi].present)
    {
        // Nothing to unmap
        return;
    }

    // Get the page table
    page_table_t* table = (page_table_t*)(
        kernel_directory.entries[pdi].frame << 12
    );
    
    // Clear the page table entry
    uint32_t* entry = (uint32_t*)&table->entries[pti];

    // Zero = not present
    *entry = 0;

    // Flush TLB for this address
    __asm__ volatile(
        "invlpg (%0)"
        : 
        : "r"(virtual_addr)
        : "memory"
    );
}

// --- get_physical -----------------------------------------
// Walks the page tables to find the physical address for a virtual address
// Returns 0 if the address is not mapped
uint32_t get_physical(uint32_t virtual_addr)
{
    uint32_t pdi = PD_INDEX(virtual_addr);
    uint32_t pti = PT_INDEX(virtual_addr);

    // Check directory entry is present
    if(!kernel_directory.entries[pdi].present)
    {
        // Not mapped
        return 0;
    }

    // Get the page table
    page_table_t* table = (page_table_t*)(
        kernel_directory.entries[pdi].frame << 12
    );

    // Check page table entry is present
    if(!table->entries[pti].present)
    {
        // Not mapped
        return 0;
    }

    // Get physical address - frame is upper 20 bits, add back the offset
    uint32_t physical = table->entries[pti].frame << 12;

    // Add the byte offset within the page
    physical |= PG_OFFSET(virtual_addr);

    return physical;
}