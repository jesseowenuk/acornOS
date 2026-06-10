#include "paging.h"
#include "pmm.h"            // For PAGE_SIZE
#include "vga.h"
#include "kprintf.h"
#include "panic.h"

// --- Page directory -----------------------------------
// We declare this statically so it lives in the kernel's BSS segment
// __attribute__((aligned(4096))) ensures it starts on a 4KB boundary
// which is required by the CPU - CR3 must point to a 4KB-aligned address
page_directory_t kernel_directory __attribute__((aligned(4096)));

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
            kpanic("map page: out of page tables!");
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

// --- map_page_in ---------------------------------------------------
// Map a page in a SPECIFIC page directory
void map_page_in(page_directory_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags)
{
    // Guard against null directory
    if(!dir)
    {
        kpanic("map_page_in: null page directory!");
    }

    virtual_addr &= ~0xFFF;
    physical_addr &= ~0xFFF;

    uint32_t pdi = PD_INDEX(virtual_addr);
    uint32_t pti = PT_INDEX(virtual_addr);

    // Check if page table exists in THIS directory
    if(!dir->entries[pdi].present)
    {
        page_table_t* new_table = alloc_table();

        if(!new_table)
        {
            kserial_printf("map_page_in: out of page tables!\n");
            return;
        }

        pde_set((pde_t*)&dir->entries[pdi], (uint32_t)new_table, PAGE_PRESENT | PAGE_WRITABLE);
    }

    page_table_t* table = (page_table_t*)(dir->entries[pdi].frame << 12);

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

// --- paging_clone_directory ---------------------------------
// Creates a new page directory for a process
// kernel mappings are shared - user mappings start empty

page_directory_t* paging_clone_directory()
{
    // Allocate a new page directory from PMM
    // Must be page aligned - PMM always returns page aligned memory
    page_directory_t* new_dir = (page_directory_t*)pmm_alloc();

    if(!new_dir)
    {
        kserial_printf("paging_clone_directory: out of memory!\n");
        return 0;
    }

    // Zero out the entire new directory
    // All entries start as not present
    // Copy kernel page directory entries into the new directory
    // We share kernel mappings between all processes
    // Entry 0 covers 0x00000000 - 0x003FFFFF (first 4MB - our kernel)
    // We copy this entry so the new process can still access kernel code
    uint32_t* raw = (uint32_t*)new_dir;
    uint32_t* kernel_raw = (uint32_t*)&kernel_directory;
    
    for(int i = 0; i < 1024; i++)
    {
        if(kernel_raw[i] & PAGE_PRESENT)
        {
            // Share ALL present kernel entries
            raw[i] = kernel_raw[i];
        }
        else
        {
            // Not present - no mappings yet
            raw[i] = 0;
        }
    }

    kserial_printf("paging: cloned directory at 0x%x\n", (uint32_t)new_dir);
    return new_dir;
}

// --- paging_switch_directory --------------------------------------------
// Loads a page directory into CR3
// Called on every context switch to switch virtual address spaces

void paging_switch_directory(page_directory_t* dir)
{
    if(!dir)
    {
        // Safety check - never load null directory
        return;
    }

    //kserial_printf("switching CR3 to 0x%x\n", (uint32_t)dir);

    // Load the physical address of the page directory into CR3
    // CR3 = Page Directory Base Register
    // CPU reads this on every TLB miss to find the page directory
    __asm__ volatile(
        "mov %0, %%cr3"                 // Load new page directory
        :                               // No output
        : "r"(dir)                      // Input: address of new directory
        : "memory"                      // Tell compiler memory layout may change
    );

    // Loading CR3 automatically flushes the entire TLB - all cached tranlations
    // are invalidated so the CPU uses the new directory immediatley.
}

// --- paging_deep_copy_directory -----------------------------------------------
// Creates a complete independent copy of a page directory
// Used by fork() to give child its own address space

page_directory_t* paging_deep_copy_directory(page_directory_t* src)
{
    // Allocate new page directory
    page_directory_t* dst = (page_directory_t*)pmm_alloc();
    if(!dst)
    {
        kserial_printf("paging_deep_copy: out of memory!\n");
        return 0;
    }

    // Zero the new directory
    uint32_t* raw = (uint32_t*)dst;
    for(int i = 0; i < 1024; i++)
    {   
        raw[i] = 0;
    }

    // Walk every entry in the source directory
    for(int i = 0; i < 1024; i++)
    {
        if(!src->entries[i].present)
        {
            // Skip non-present entries
            continue;
        }

        // Get the source page table
        page_table_t* src_table = (page_table_t*)(src->entries[i].frame << 12);

        // Check if this is a kernel mapping
        // Kernel pages are in the first entry (first 4MB)
        // We share these so no need to copy
        if(i == 0)
        {
            // Share kernel mapping directly
            dst->entries[i] = src->entries[i];
            continue;
        }

        // User space page table - deep copy it
        // Allocate a new page table
        page_table_t* dst_table = (page_table_t*)pmm_alloc();
        if(!dst_table)
        {
            // TODO: free allocated pages
            kserial_printf("paging_deep_copy: out of memory for table!\n");
            return 0;
        }

        // Zero the new table
        uint32_t* t = (uint32_t*)dst_table;
        for(int j = 0; j < 1024; j++)
        {   
            t[j] = 0;
        }

        // Walk every entry in the source page table
        for(int j = 0; j < 1024; j++)
        {
            if(!src_table->entries[j].present)
            {
                // skip non-present entries
                continue;
            }

            // Allocate a new physical page for the child
            uint32_t new_page = (uint32_t)pmm_alloc();
            if(!new_page)
            {
                kserial_printf("paging_deep_copy: out of memory for page!\n");
                return 0;
            }

            // Copy contents of source page to new page
            uint8_t* src_data = (uint8_t*)(src_table->entries[j].frame << 12);
            uint8_t* dst_data = (uint8_t*)new_page;

            for(int k = 0; k < PAGE_SIZE; k++)
            {
                // Copy every byte
                dst_data[k] = src_data[k];
            }

            // Set up the new page table entry
            // Same flags as source but pointing to new physical page
            pte_set(
                (pte_t*)&dst_table->entries[j],
                new_page,
                PAGE_PRESENT | PAGE_WRITABLE | (src_table->entries[j].user ? PAGE_USER : 0)
            );
        }

        // Install new page table into destination directory
        pde_set(
            (pde_t*)&dst->entries[i],
            (uint32_t)dst_table,
            PAGE_PRESENT | PAGE_WRITABLE | (src->entries[i].user ? PAGE_USER : 0)
        );
    }

    kserial_printf("paging: deep copied directory to 0x%x\n", (uint32_t)dst);
    return dst;
}