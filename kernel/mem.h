#ifndef MEM_H
#define MEM_H

#include <stdint.h>

// Physical address of the heap
#define HEAP_PHYS 0xFFFF800002200000UL

// Start of the heap
#define HEAP_START 0xFFFF800002200000UL

// Total heap size - 16MB
#define HEAP_SIZE (16 * 1024 * 1024)

// Initialise the heap - must be called before kmalloc/kfree
void mem_init();

// Allocate 'size' bytes - returns pointer to usable memory
// Returns 0 (NULL) if no block large enough is available
void* kmalloc(uint64_t size);

// Free a previously allocated block
// Passing a NULL pointer is safe - it does nothing
void kfree(void* ptr);

// Print a summary of heap usage to VGA - useful for debugging
void mem_print_stats();

#endif