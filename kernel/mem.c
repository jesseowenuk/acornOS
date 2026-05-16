#include "mem.h"
#include "vga.h"        // For mem_print_stats() output

// --- Block header --------------------------------------
// Every allocation is preceded by this header in memory
// The layout looks like:
//  [header][..........data...........][header][....data.....][header][...data...]
//
typedef struct block_header
{
    uint32_t size;              // Size of the DATA region (not including the header)
    uint32_t free;              // 1 = this block is free, 0 = in use
    struct block_header* next;  // Pointer to the next block header in the list
                                // NULL means this is the last block
} block_header_t;

// Size of the header itself - we need to account for this in allocations
#define HEADER_SIZE sizeof(block_header_t)

// --- Heap state --------------------------------------

static block_header_t* heap_start = 0;      // Pointer to first block in the list
static uint32_t heap_used = 0;              // How many bytes are currently allocated

// --- Helper: print a number --------------------------------------
// We have no printf yet so we need to convert numbers to strings manually

static void print_num(uint32_t n)
{
    if(n == 0)
    {
        vga_putchar('0');
        return;
    }

    char buf[10];                       // Max 10 digits in a 32-bit number
    int i = 0;

    while(n > 0)
    {
        // Extract last digit
        buf[i++] = '0' + (n % 10);

        // Remove last digit
        n /= 10;
    }

    // Print digits in reverse
    while(i > 0)
    {
        vga_putchar(buf[--i]);
    }
}

// --- Init --------------------------------------

void mem_init()
{
    // Place the first block header at the start of the heap
    heap_start = (block_header_t*)HEAP_START;

    // Initialise it as one giant free block covering the whole heap
    // Entire heap minus the header itself
    heap_start->size = HEAP_SIZE - HEADER_SIZE;
    
    // Set the initial block as free
    heap_start->free = 1;

    // No next block - this is the only one
    heap_start->next = 0;
}

// --- kmalloc --------------------------------------

void* kmalloc(uint32_t size)
{
    // Allocating zero bytes makes no sense
    if(size == 0)
    {
        return 0;
    }
    
    // Align size to 4 bytes - keeps allocations aligned for the CPU
    // e.g. requesting 5 bytes gives you 8, requesting 7 gives you 8
    if(size % 4 != 0)
    {
        // Round up to next multiple of 4
        size += 4 - (size % 4);
    }

    // Walk the free list looking for a block that's big enough
    block_header_t* current = heap_start;

    vga_print("DEBUG: walking free list...\n");

    // While there are blocks to check
    while(current != 0)
    {
        if(current->free && current->size >= size)
        {
            // Found a free block big enough
            // Split the block if there's enough room left over
            // Only split if the leftover would be useful (> header + 4 bytes)
            if(current->size > size + HEADER_SIZE + 4)
            {
                // Create a new header immediatley after our allocation
                // Cast to uint8_t* for byte arithmetic then jump past header
                block_header_t* new_block = (block_header_t*)((uint8_t*)current + HEADER_SIZE + size);

                // Remaining size after our allocation
                new_block->size = current->size - size - HEADER_SIZE;

                // New block is free
                new_block->free = 1;

                // New block takes over the next pointer
                new_block->next = current->next;

                // Shrink current block to exact size
                current->size = size;

                // Link current to the new split block
                current->next = new_block;
            }

            // Mark block as in use
            current->free = 0;

            // Track usage
            heap_used += current->size;

            // Return pointer to the data region - just after the header
            return (void*)((uint8_t*)current + HEADER_SIZE);
        }

        // Move to next block and keep looking
        current = current->next;
    }

    // No block found - out of memory
    return 0;
}

// --- kfree --------------------------------------

void kfree(void* ptr)
{
    // Freeing NULL is safe - do nothing
    if(ptr == 0)
    {
        return;
    }

    // The header lives immediatley before the data pointer
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);

    block->free = 1;            // Mark block as free
    heap_used -= block->size;   // Update usage counter

    // Coalesce - merge adjacent free blocks to prevent fragmentation
    // Walk from the start and merge any two consective free blocks
    block_header_t* current = heap_start;
    while(current != 0 && current->next != 0)
    {
        if(current->free && current->next->free)
        {
            // Both this block and the next are free
            
            // Absorb the next block's header + data
            current->size += HEADER_SIZE + current->next->size;

            // Skip over the absorbed block
            current->next = current->next->next;
        }
        else
        {
            // Only advance if we didn't merge
            // (merging might allow another merge)
            current = current->next;
        }
    }
}

// --- Stats --------------------------------------

void mem_print_stats()
{
    vga_set_colour(CYAN, BLACK);
    vga_print("\nMemory stats:\n");
    vga_set_colour(WHITE, BLACK);

    // Count blocks and free space by walking the list
    uint32_t total_blocks = 0;
    uint32_t free_blocks = 0;
    uint32_t free_bytes = 0;

    block_header_t* current = heap_start;
    while(current != 0)
    {
        total_blocks++;

        if(current->free)
        {
            free_blocks++;
            free_bytes += current->size;
        }
        current = current->next;
    }

    vga_print("    Heap start : 0x100000\n");
    vga_print("    Total size : ");
    print_num(HEAP_SIZE / 1024);
    vga_print("KB\n");
    vga_print("    Used       : ");
    print_num(heap_used);
    vga_print(" bytes\n");
    vga_print("    Free       : ");
    print_num(free_bytes);
    vga_print(" bytes\n");
    vga_print("    Blocks     : ");
    print_num(total_blocks);
    vga_print("\n");
}