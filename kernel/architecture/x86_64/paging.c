#include <architecture/x86_64/idt.h>
#include <architecture/x86_64/paging.h>
#include <drivers/vga.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/pmm.h>

// --- Kernel PML4 --------------------------------------------------
// Pointer to the kernel's top-level page table
// Set up by Stage 2 at physical 0x1000
// We read CR3 at boot to find it
page_directory_t* kernel_pml4 = 0;

// --- Physical / Virtual conversion --------------------------------
// All physical RAM is mapped via direct map at PHYSICAL_MAP_BASE   
// We use this to access page table entries by physical address
static inline page_table_t* physical_to_table(uint64_t physical)
{
    return (page_table_t*)PHYSICAL_TO_VIRTUAL(physical);
}

// --- Physical to virtual -------------------------------------------
uint64_t physical_to_virtual(uint64_t physical)
{
    return physical + PHYSICAL_MAP_BASE;
}

// --- Virtual to physical -------------------------------------------
uint64_t virtual_to_physical(uint64_t virtual)
{
    return virtual - PHYSICAL_MAP_BASE;
}


// --- Allocate a zeroed page table ----------------------------------
static page_table_t* alloc_table()
{
    // PMM returns physcial address
    uint64_t physical = (uint64_t)pmm_alloc();
    if(!physical)
    {
        kpanic("paging: out of memory for page table!");
    }

    // Access via direct map to zero it
    page_table_t* table = physical_to_table(physical);
    for(int i = 0; i < 512; i++)
    {
        // Zero every entry - not present
        *((uint64_t*)&table->entries[i]) = 0;
    }

    return table;
}

// --- Get physical address of a table pointer -------------------------
static inline uint64_t table_to_physical(page_table_t* table)
{
    return VIRTUAL_TO_PHYSICAL((uint64_t)table);
}

// --- Walk/create page table hierarchy ---------------------------------
// Given a PML4 and virtual address, walk down to the PT entry
// Creates intermediate tables if they don't exist (when create=1)
// Returns pointer to the PT entry, or NULL if not present and create=0
static page_entry_t* get_pt_entry(page_directory_t* pml4, uint64_t vaddr, int create)
{
    // Level 1: PML4
    uint64_t pml4_idx = PML4_INDEX(vaddr);
    page_entry_t* pml4_entry = &pml4->entries[pml4_idx];

    if(!pml4_entry->present)
    {
        if(!create)
        {
            return 0;
        }

        // Allocate PDPT
        page_table_t* pdpt = alloc_table();
        *((uint64_t*)pml4_entry) = 0;
        pml4_entry->frame = table_to_physical(pdpt) >> 12;
        pml4_entry->present = 1;
        pml4_entry->writable = 1;
        pml4_entry->user = 1;
    }

    // Level 2: PDPT
    page_table_t* pdpt = physical_to_table((uint64_t)pml4_entry->frame << 12);
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    page_entry_t* pdpt_entry = &pdpt->entries[pdpt_idx];

    if(!pdpt_entry->present)
    {
        if(!create)
        {
            return 0;
        }

        // Allocate PD
        page_table_t* pd = alloc_table();
        *((uint64_t*)pdpt_entry) = 0;
        pdpt_entry->frame = table_to_physical(pd) >> 12;
        pdpt_entry->present = 1;
        pdpt_entry->writable = 1;
        pdpt_entry->user = 1;
    }

    // Level 3: PD
    page_table_t* pd = physical_to_table((uint64_t)pdpt_entry->frame << 12);
    uint64_t pd_idx = PD_INDEX(vaddr);
    page_entry_t* pd_entry = &pd->entries[pd_idx];

    if(!pd_entry->present)
    {
        if(!create)
        {
            return 0;
        }

        // Allocate PD
        page_table_t* pt = alloc_table();
        *((uint64_t*)pd_entry) = 0;
        pd_entry->frame = table_to_physical(pt) >> 12;
        pd_entry->present = 1;
        pd_entry->writable = 1;
        pd_entry->user = 1;
    }

    // Level 4: PT
    page_table_t* pt = physical_to_table((uint64_t)pd_entry->frame << 12);
    uint64_t pt_idx = PT_INDEX(vaddr);

    return &pt->entries[pt_idx];
}

// --- paging_init -------------------------------------------------------
// Called after kernel boots - find Stage 2 page tables via CR3
void paging_init()
{
    uint64_t efer;
    __asm__ volatile("rdmsr" : "=A"(efer) : "c"(0xC0000080UL));
    kserial_printf("paging: EFER=0x%lx NXE=%d\n", efer, (int)((efer >> 11) & 1));

    // Read CR3 to find the PML4 Stage 2 set up
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    // CR3 holds physical address of PML4
    // Access it via direct map
    kernel_pml4 = (page_directory_t*)PHYSICAL_TO_VIRTUAL(cr3);

    kserial_printf("Paging kernel PML4 at phys=0x%lx virt=0x%lx\n", cr3, (uint64_t)kernel_pml4);
    kserial_printf("Paging: using Stage 2 page tables (4-levels)\n");
}

// --- map_page ----------------------------------------------------------
// Map a single 4KB virtual page to a physical page
void map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    vaddr &= ~0xFFFUL;       // Align to 4KB
    paddr &= ~0xFFFUL;

    page_entry_t* entry = get_pt_entry(kernel_pml4, vaddr, 1);

    if(!entry)
    {
        kpanic("map_page: failed to get page table entry!");
    }

    *((uint64_t*)entry) = 0;
    entry->frame = paddr >> 12;
    entry->present = (flags & PAGE_PRESENT) ? 1 : 0;
    entry->writable = (flags & PAGE_WRITABLE) ? 1 : 0;
    entry->user = (flags & PAGE_USER) ? 1 : 0;

    // Flush TLB entry
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

// --- map_page_in ----------------------------------------------------------
// Map a page in a specific address space
void map_page_in(page_directory_t* pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    if(!pml4)
    {
        kpanic("map_page_in: null PML4!");
    }

    vaddr &= ~0xFFFUL;       // Align to 4KB
    paddr &= ~0xFFFUL;

    page_entry_t* entry = get_pt_entry(pml4, vaddr, 1);

    if(!entry)
    {
        kpanic("map_page_in: failed to get page table entry!");
    }

    *((uint64_t*)entry) = 0;
    entry->frame = paddr >> 12;
    entry->present = (flags & PAGE_PRESENT) ? 1 : 0;
    entry->writable = (flags & PAGE_WRITABLE) ? 1 : 0;
    entry->user = (flags & PAGE_USER) ? 1 : 0;

    // Flush TLB entry
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

// --- unmap_page ------------------------------------------------------------
void unmap_page(uint64_t vaddr)
{
    vaddr &= ~0xFFUL;

    page_entry_t* entry = get_pt_entry(kernel_pml4, vaddr, 0);
    if(!entry)
    {
        // Nothing mapped - so nothing to do
        return;
    }

    // Clear entry
    *((uint64_t*)entry) = 0;

    __asm__ volatile("invlpg (%0)" : : "r"(vaddr): "memory");
}

// --- get_physical -------------------------------------------------------
uint64_t get_physical(uint64_t vaddr)
{
    page_entry_t* entry = get_pt_entry(kernel_pml4, vaddr, 0);
    if(!entry || !entry->present)
    {
        return 0;
    }

    return ((uint64_t)entry->frame << 12) | PG_OFFSET(vaddr);
}

// --- get_physical_in -----------------------------------------------------
// Walk a specific page directory to find physical address
uint64_t get_physical_in(page_directory_t* pml4, uint64_t vaddr)
{
    page_entry_t* entry = get_pt_entry((page_directory_t*)pml4, vaddr, 0);
    if(!entry || !entry->present)
    {
        return 0;
    }

    return ((uint64_t)entry->frame << 12) | PG_OFFSET(vaddr);
}

// --- paging_clone_directory ----------------------------------------------
// Create a new address space sharing kernel mappings
page_directory_t* paging_clone_directory()
{
    // Allocate a new PML4
    page_table_t* new_pml4 = alloc_table();

    // Copy kernel mappings (upper half, indicies 256-511)
    // User space (indicies 0-255) starts empty
    for(int i = 256; i < 512; i++)
    {
        new_pml4->entries[i] = kernel_pml4->entries[i];
    }

    kserial_printf("paging: cloned PML4 at phys=0x%lx\n", table_to_physical(new_pml4));

    return (page_directory_t*)new_pml4;
}

// --- paging_switch_directory ---------------------------------------
void paging_switch_directory(page_directory_t* pml4)
{
    if(!pml4)
    {
        // NULL = keep current
        return;
    }

    uint64_t physical = VIRTUAL_TO_PHYSICAL((uint64_t)pml4);

    kserial_printf("paging_switch: virt=0x%lx phys=0x%lx\n", (uint64_t)pml4, physical);

    __asm__ volatile("mov %0, %%cr3" : : "r"(physical) : "memory");
}

// --- paging_deep_copy_directory -------------------------------------
// Full copy of address space for fork()
page_directory_t* paging_deep_copy_directory(page_directory_t* src)
{
    if(!src)
    {
        kpanic("paging_deep_copy: null source!");
    }

    kserial_printf("paging_deep_copy: starting\n");

    // Allocate new PML4
    page_table_t* dst_pml4 = alloc_table();
    kserial_printf("paging_deep_copy: allocated dst_pml4\n");

    // Copy kernel mappings (shared, upper half)
    for(int i = 256; i < 512; i++)
    {
        dst_pml4->entries[i] = ((page_table_t*)src)->entries[i];
    }

    // Deep copy user space (lower half: indicies 0-255)
    for(int pml4_i = 0; pml4_i < 256; pml4_i++)
    {
        page_entry_t* src_pml4_e = &((page_table_t*)src)->entries[pml4_i];
        if(!src_pml4_e->present)
        {
            continue;
        }

        // Allocate new PDPT
        page_table_t* dst_pdpt = alloc_table();
        dst_pml4->entries[pml4_i] = *src_pml4_e;
        dst_pml4->entries[pml4_i].frame = table_to_physical(dst_pdpt) >> 12;

        page_table_t* src_pdpt = physical_to_table((uint64_t)src_pml4_e->frame << 12);

        for(int pdpt_i = 0; pdpt_i < 512; pdpt_i++)
        {
            page_entry_t* src_pdpt_e = &src_pdpt->entries[pdpt_i];
            if(!src_pdpt_e->present)
            {
                continue;
            }

            // Allocate new PD
            page_table_t* dst_pd = alloc_table();
            dst_pd->entries[pdpt_i] = *src_pdpt_e;
            dst_pd->entries[pdpt_i].frame = table_to_physical(dst_pd) >> 12;

            page_table_t* src_pd = physical_to_table((uint64_t)src_pdpt_e->frame << 12);

            for(int pd_i = 0; pd_i < 512; pd_i++)
            {
                page_entry_t* src_pd_e = &src_pd->entries[pd_i];
                if(!src_pd_e->present)
                {
                    continue;
                }

                if(src_pd_e->huge_page)
                {
                    // 2MB page - share it (copy on write later)
                    dst_pd->entries[pd_i] = *src_pd_e;
                    continue;
                }

                // Allocate new PT
                page_table_t* dst_pt = alloc_table();
                dst_pt->entries[pd_i] = *src_pd_e;
                dst_pt->entries[pd_i].frame = table_to_physical(dst_pt) >> 12;

                page_table_t* src_pt = physical_to_table((uint64_t)src_pd_e->frame << 12);

                for(int pt_i = 0; pt_i < 512; pt_i++)
                {
                    page_entry_t* src_pt_e = &src_pt->entries[pt_i];

                    if(!src_pt_e->present)
                    {
                        continue;
                    }

                    // Allocate new physical page and copy contents
                    uint64_t new_physical = (uint64_t)pmm_alloc();
                    if(!new_physical)
                    {
                        kpanic("paging_deep_copy: out of memory!");
                    }

                    // Copy page contents via direct map
                    uint8_t* src_page = (uint8_t*)PHYSICAL_TO_VIRTUAL((uint64_t)src_pt_e->frame << 12);
                    uint8_t* dst_page = (uint8_t*)PHYSICAL_TO_VIRTUAL(new_physical);

                    for(int b = 0; b < 4096; b++)
                    {
                        dst_page[b] = src_page[b];
                    }

                    dst_pt->entries[pt_i] = *src_pt_e;
                    dst_pt->entries[pt_i].frame = new_physical >> 12;
                }
            }
        }
    }

    kserial_printf("paging: deep copied PML4 to phys=0x%lx\n", table_to_physical(dst_pml4));

    return (page_directory_t*)dst_pml4;
}

// --- Page fault handler ----------------------------------------
void page_fault_handler(registers_t* regs)
{
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    uint64_t cr3;
    __asm__ volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
    );
    kserial_printf("PAGE FAULT: CR3=0x%lx\n", cr3);

    int present = regs->err_code & 0x1;
    int write = (regs->err_code >> 1) & 0x1;
    int user = (regs->err_code >> 2) & 0x1;

    vga_set_colour(RED, BLACK);
    kprintf("\n--- PAGE FAULT ---\n");
    vga_set_colour(WHITE, BLACK);
    kprintf("Address : 0x%lx\n", faulting_address);
    kprintf("Reason  : %s %s %s\n",
        present ? "protection violation" : "page not present",
        write   ? "on write"             : "on read",
        user    ? "from user space"      : "from kernel");

    kserial_printf("PAGE FAULT at 0x%lx\n", faulting_address);
    kserial_printf("Error: present=%d write=%d user=%d\n", present, write, user);
    kserial_printf("RIP: 0x%lx\n", regs->rip);

    for(;;);
}