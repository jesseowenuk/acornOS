#include "pmm.h"
#include "vga.h"
#include "serial.h"
#include "kprintf.h"
#include "panic.h"
#include "mem.h"

// --- Bitmap ---------------------------------------------
// The bitmap is stored at PMM_BITMAP_ADDRESS
// Each bit = one 4KB page. Bit 0 of byte 0 = page 0 (address 0x0000)
//                          Bit 1 of byte 0 = page 1 (address 0x1000) etc.

// Pointer to the start of our bitmap in memory
static uint8_t* bitmap = (uint8_t*)PMM_BITMAP_ADDRESS;

// Total pages in the system
static uint64_t total_pages = 0;

// Pages currently allocated
static uint64_t used_pages = 0;

// --- Bitmap helpers ---------------------------------------------

// Mark a page as used - set its bit to 1
static void bitmap_set(uint64_t page)
{
    // page / 8 gives us which byte this page's bit lives in
    // page % 8 gives us which bit within that byte
    // 1 << (page % 8) creates a mask with just that bit set
    // |= sets that bit without touching the others
    bitmap[page / 8] |= (1 << (page % 8));
}

// Mark a page as free - clear its bit to 0
static void bitmap_clear(uint64_t page)
{
    // ~(1 << (page % 8)) creates a mask with all bits set EXCEPT that one
    // &= clears just that bit without touching the others
    bitmap[page / 8] &= ~(1 << (page % 8));
}

// Check if a page is free - returns 1 if free, 0 if used
static int bitmap_test(uint64_t page)
{
    // & isolates the bit we care about
    // ! inverts it - bit 0 means free so we can return 1 for free
    return !(bitmap[page / 8] & (1 << (page % 8)));
}

// --- Init ---------------------------------------------
void pmm_init(uint64_t mem_map_addr, uint64_t mem_map_count, uint64_t highest_ram)
{
    kserial_printf("PMM: E820 map address=0x%x\n", (uint32_t)mem_map_addr);
    kserial_printf("PMM: first entry type=%u\n", ((e820_entry_t*)(uintptr_t)mem_map_addr)->type);

    kserial_printf("PMM: highest_ram=%u\n", (uint32_t)highest_ram);
    kserial_printf("PMM: mem_map_count=%u\n", (uint32_t)mem_map_count);

    // Step 1: get the total memory size from the E820 map
    // We find the highest usable address to know how many pages we need

    // Cast the raw address to our entry struct pointer
    e820_entry_t* map = (e820_entry_t*)(uintptr_t)mem_map_addr;

    // Use highest_ram passed from stage 2 bootloader
    total_pages = highest_ram / PAGE_SIZE;
    kserial_printf("PMM: total_pages=%u\n", (uint32_t)total_pages);

    // Step 3: mark ALL pages as used to start with
    // We'll then mark usable regions as free
    // This is safer than the reverse - unknown regions stay reserved

    // How many bytes the bitmap needs
    uint64_t bitmap_bytes = total_pages / 8 + 1;
    kserial_printf("PMM: bitmap_bytes=%u\n", (uint32_t)bitmap_bytes);

    kserial_printf("PMM: bitmap at 0x%x\n", (uint32_t)PMM_BITMAP_ADDRESS);
    kserial_printf("PMM: about to clear bitmap...\n");

    kserial_printf("PMM: bitmap address = 0x%x%x\n", (uint32_t)((uint64_t)bitmap >> 32), (uint32_t)(uint64_t)bitmap);

    for(uint64_t i = 0; i < bitmap_bytes; i++)
    {
        // 0xFF = all bits set = all pages used
        bitmap[i] = 0xFF;
    }

    kserial_printf("PMM: bitmap cleared\n");

    // All pages start as used
    used_pages = total_pages;

    kserial_printf("PMM: about to walk E820...\n");

    // Step 4: Walk E820 map again, mark usable regions as free
    for(uint64_t i = 0; i < mem_map_count; i++)
    {
        kserial_printf("PMM: E820 entry %u type=%u\n", (uint32_t)i, map[i].type);

        if(map[i].type == E820_USABLE)
        {
            uint64_t base = (uint64_t)map[i].base;

            // How many full pages fit in this region
            uint64_t pages = (uint64_t)map[i].length / PAGE_SIZE;

            kserial_printf("PMM: marking %u pages free from 0x%x\n", (uint32_t)pages, (uint32_t)base);

            for(uint64_t p = 0; p < pages; p++)
            {
                // Page number = address / page size
                uint64_t page = (base / PAGE_SIZE) + p;

                // Mark this page as free
                bitmap_clear(page);

                // Decrement used count
                used_pages--;
            }
        }
    }

    // Step 5: Mark critical regions as used so we never overwrite them
    // These are regions our kernel and data structures live in

    // Mark page 0 as used - null pointer protection
    // Any code that accidentally dereferences NULL will fault cleanly
    kserial_printf("PMM: marking reserved regions...\n");

    bitmap_set(0);
    used_pages++;
    kserial_printf("PMM: null page reserved\n");
    
    // Mark pages covering our bootloader (0x7C00 - 0x7E00)
    bitmap_set(0x7C00 / PAGE_SIZE);
    used_pages++;
    kserial_printf("PMM: bootloader reserved\n");

    // Mark pages covering our kernel (0x1000 - 0x20000)
    for(uint64_t addr = 0x1000; addr < 0x20000; addr += PAGE_SIZE)
    {
        bitmap_set(addr / PAGE_SIZE);
        used_pages++;
    }
    kserial_printf("PMM: kernel pages reserved\n");

    // Mark pages covering our bitmap itself so we don't allocate over it
    kserial_printf("PMM: reserving bitmap pages...\n");
    uint64_t bitmap_pages = bitmap_bytes / PAGE_SIZE + 1;
    kserial_printf("PMM: bitmap_pages=%u\n", (uint32_t)bitmap_pages);

    for(uint64_t p = 0; p < bitmap_pages; p++)
    {
        bitmap_set((PMM_BITMAP_PHYSICAL / PAGE_SIZE) + p);
        used_pages++;
    }
    kserial_printf("PMM: bitmap pages reserved\n");

    // Mark pages covering our heap
    for(uint64_t addr = 0x2200000; addr < 0x2200000 + HEAP_SIZE; addr += PAGE_SIZE)
    {
        bitmap_set(addr / PAGE_SIZE);
        used_pages++;
    }

    // Log results to serial
    kserial_printf("PMM: %u free pages, %u used pages.\n", total_pages - used_pages, used_pages);
}

// --- Alloc --------------------------------------------------

void* pmm_alloc()
{
    // Walk the bitmap looking for a free page
    for(uint64_t i = 0; i < total_pages; i++)
    {
        // If page free
        if(bitmap_test(i))
        {
            // mark it as used
            bitmap_set(i);

            // Update counter
            used_pages++;

            // Return its physical address
            return (void*)((uint64_t)i * PAGE_SIZE);
        }
    }

    // No free pages - out of memory
    kpanic("pmm_alloc: out of physical memory!");
    return 0;
}

// --- Free --------------------------------------------------
void pmm_free(void* page)
{
    if(page == 0)
    {
        // Never free the null page
        return;
    }

    // Convert address back to page number
    uint64_t page_num = (uint64_t)page / PAGE_SIZE;

    // Mark as free
    bitmap_clear(page_num);

    // Update counter
    used_pages--;
}

// --- Stats --------------------------------------------------
uint64_t pmm_free_pages()
{
    // Free = total minus used
    return total_pages - used_pages;
}

uint64_t pmm_used_pages()
{
    return used_pages;
}

void pmm_print_stats()
{
    vga_set_colour(CYAN, BLACK);
    kprintf("\nPhysical Memory:\n");
    vga_set_colour(WHITE, BLACK);
    kprintf("    Total pages : %u (%uMB)\n", total_pages, total_pages * PAGE_SIZE / 1024 /1024);
    kprintf("    Used pages  : %u (%uKB)\n", used_pages, used_pages * PAGE_SIZE / 1024);
    kprintf("    Free pages  : %u (%uMB)\n", total_pages - used_pages, (total_pages - used_pages) * PAGE_SIZE / 1024 / 1024);
}