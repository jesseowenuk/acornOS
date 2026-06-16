#include <architecture/x86_64/idt.h>
#include <architecture/x86_64/paging.h>
#include <drivers/vga.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/pmm.h>

// --- Page directory -----------------------------------
// We declare this statically so it lives in the kernel's BSS segment
// __attribute__((aligned(4096))) ensures it starts on a 4KB boundary
// which is required by the CPU - CR3 must point to a 4KB-aligned address
page_directory_t kernel_directory __attribute__((aligned(4096)));

// --- Page table ----------------------------------------
// We need one page table for every 4MB of virtual space we want to map
// Each page table covers 1024 pages x 4KB = 4MB
// For now we'll identity map the first 4MB - enough for our kernel
//static page_table_t first_table __attribute__((aligned(4096)));

// --- Helper: Set a page directory entry ------------------------
// Takes a PTE pointer, physical address of the page table and flags
static void pte_set(pte_t* entry, uint64_t phsical_addr, uint64_t flags)
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
static void pde_set(pde_t* entry, uint64_t table_phsical_addr, uint64_t flags)
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
    kserial_printf("Paging using stage 2 page tables (64-bit)\n");
    // Page tables already set up by stage 2 bootloader
    // TODO: rebuild proper kernel page tables here
}

// --- Page fault handler ---------------------------------------
// Called by isr_handler in idt.c when interrupt 14 fires
// regs->err_code contains the fault flags
// CR2 contains the virtual address that caused the fault
void page_fault_handler(registers_t* regs)
{
    // Read CR2 - the CPU stores the faulting address here automatically
    uint64_t faulting_addr;
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
    kserial_printf("PAGE FAULT at 0x%x%x\n", (uint32_t)(faulting_addr >> 32), (uint32_t)faulting_addr);
    kserial_printf("Error: present=%d, write=%d, user=%d\n",
        regs->err_code & 1,
        (regs->err_code >> 1) & 1,
        (regs->err_code >> 2) & 1);

    kserial_printf("RIP: 0x%x%x\n",
        (uint32_t)(regs->rip >> 32),
        (uint32_t)regs->rip);

    // Hang - we can't safely continue after a page fault
    for(;;);
}

// --- Page table storage ------------------------------------

// Allocate a fresh zeroed page table from our pool
static page_table_t* alloc_table()
{
    // PMM gives us a free 4KB physical page - exactly the right size
    page_table_t* table = (page_table_t*)pmm_alloc();

    // Zero out every entry - all pages start out as not present
    uint64_t* t = (uint64_t*)table;
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
void map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags)
{
    // Round both addresses down to page boundaries
    // e.g. 0x1234 becomes 0x1000 - we always map whole pages
    // Clear bottom 12 bits
    virtual_addr &= ~0xFFF;
    physical_addr &= ~0xFFF;

    // Get the page directory index from the virtual address
    // Which entry in the page directory?
    uint64_t pdi = PD_INDEX(virtual_addr);

    // Get the page table index from the virtual address
    // Which entry in the page table?
    uint64_t pti = PT_INDEX(virtual_addr);

    // Check if a page table already exists in this direcrory entry
    if(!kernel_directory.entries[pdi].present)
    {
        // No page table yet so lets create one
        page_table_t* new_table = alloc_table();

        if(!new_table)
        {
            kpanic("map page: out of page tables!");
            return;
        }

        // Install the new table into the directory
        pde_set(
            (pde_t*)&kernel_directory.entries[pdi],
            (uint64_t)new_table,                // physical address of the new table
            PAGE_PRESENT | PAGE_WRITABLE
        );
    }

    // Get pointer to the page table for this directory entry
    // frame contains the upper 20 bits of the table address
    // shift left 12 to get the full address back
    page_table_t* table = (page_table_t*)(
        (uint64_t)kernel_directory.entries[pdi].frame << 12
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

// --- map_page_in ---------------------------------------------------
// Map a page in a SPECIFIC page directory
void map_page_in(page_directory_t* dir, uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags)
{
    // Guard against null directory
    if(!dir)
    {
        kpanic("map_page_in: null page directory!");
    }

    virtual_addr &= ~0xFFF;
    physical_addr &= ~0xFFF;

    uint64_t pdi = PD_INDEX(virtual_addr);
    uint64_t pti = PT_INDEX(virtual_addr);

    // Check if page table exists in THIS directory
    if(!dir->entries[pdi].present)
    {
        page_table_t* new_table = alloc_table();

        if(!new_table)
        {
            kpanic("map_page_in: out of page tables!");
            return;
        }

        pde_set((pde_t*)&dir->entries[pdi], (uint64_t)new_table, PAGE_PRESENT | PAGE_WRITABLE);
    }

    page_table_t* table = (page_table_t*)((uint64_t)dir->entries[pdi].frame << 12);

    pte_set((pte_t*)&table->entries[pti], physical_addr, flags);

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_addr)
        : "memory"
    );
}

// --- unmap_page ----------------------------------------------------
// Remove a virtual address mapping
void unmap_page(uint64_t virtual_addr)
{
    virtual_addr &= ~0xFFF;                 // Round to page boundary

    uint64_t pdi = PD_INDEX(virtual_addr);
    uint64_t pti = PT_INDEX(virtual_addr);

    // Check directory entry exists
    if(!kernel_directory.entries[pdi].present)
    {
        // Nothing to unmap
        return;
    }

    // Get the page table
    page_table_t* table = (page_table_t*)(
        (uint64_t)kernel_directory.entries[pdi].frame << 12
    );
    
    // Clear the page table entry
    uint64_t* entry = (uint64_t*)&table->entries[pti];

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
uint64_t get_physical(uint64_t virtual_addr)
{
    uint64_t pdi = PD_INDEX(virtual_addr);
    uint64_t pti = PT_INDEX(virtual_addr);

    // Check directory entry is present
    if(!kernel_directory.entries[pdi].present)
    {
        // Not mapped
        return 0;
    }

    // Get the page table
    page_table_t* table = (page_table_t*)(
        (uint64_t)kernel_directory.entries[pdi].frame << 12
    );

    // Check page table entry is present
    if(!table->entries[pti].present)
    {
        // Not mapped
        return 0;
    }

    // Get physical address - frame is upper 20 bits, add back the offset
    uint64_t physical = (uint64_t)table->entries[pti].frame << 12;

    // Add the byte offset within the page
    physical |= PG_OFFSET(virtual_addr);

    return physical;
}

// --- paging_clone_directory ---------------------------------
// Creates a new page directory for a process
// kernel mappings are shared - user mappings start empty

page_directory_t* paging_clone_directory()
{
    kserial_printf("paging_clone: stub - returning NULL\n");
    return 0;
}

// --- paging_switch_directory --------------------------------------------
// Loads a page directory into CR3
// Called on every context switch to switch virtual address spaces

void paging_switch_directory(page_directory_t* dir)
{
    (void)dir;
    // Don't switch - use stage 2 page tables for now
}

// --- paging_deep_copy_directory -----------------------------------------------
// Creates a complete independent copy of a page directory
// Used by fork() to give child its own address space

page_directory_t* paging_deep_copy_directory(page_directory_t* src)
{
    (void)src;
    kserial_printf("paging_deep_copy: stub - returning NULL");
    return 0;
}