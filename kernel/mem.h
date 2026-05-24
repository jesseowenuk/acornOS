#ifndef MEM_H
#define MEM_H

#include <stdint.h>

// Start of the heap - 1MB mark, safely above kernel and stack
// Start above PMM bitmap at 0x40000
#define HEAP_START 0x50000

// Total heap size - 256KB enough for now
#define HEAP_SIZE 0x40000

// Initialise the heap - must be called before kmalloc/kfree
void mem_init();

// Allocate 'size' bytes - returns pointer to usable memory
// Returns 0 (NULL) if no block large enough is available
void* kmalloc(uint32_t size);

// Free a previously allocated block
// Passing a NULL pointer is safe - it does nothing
void kfree(void* ptr);

// Print a summary of heap usage to VGA - useful for debugging
void mem_print_stats();

#endif