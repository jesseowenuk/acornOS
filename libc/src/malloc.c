#include <acorn/syscall.h>
#include <stdint.h>
#include <stdlib.h>

// --- A basic free-list allocator ------------------------------------------------
//
// Every block of heap memory - free or in use - has a small header right
// before it.   
//
//  [ block_header_t ][ usable data ][ block_header_t ][ usable data ] ...
//
// Blocks are kept in ONE singly linked list, in address order (the order
// they appear in memory), not split into seperate "free" and "used"
// lists. Each header just carries a flag saying whether it's currently 
// free. Because the list is built by only ever appending contiguous 
// heap-grown memory or splitting an existing block in place, 'next' is
// always the block physically immediatley after this one - there are
// never gaps. That's what makes coalesing in free() safe below: if
// block->next is free, it guaranteed to be adjacent, not just "next
// in the list".
//
// malloc() walks the list looking for the first free block big enough
// (first-fit). If none exists, it asks the kernel for more memory via
// SYS_HEAP_GROW and appends a new block. free() just flips the flag
// back and merges forward with the next block if that one is free too.

typedef struct block_header
{
    size_t size;                        // usable data size in bytes (excludes this header)
    struct block_header* next;          // next block in the heap, in address order
    int free;                           // 1 = free, 0 = in use
} block_header_t;

// First block in the heap (address order). NULL until the first malloc().
static block_header_t* heap_head = 0;

// Ask the kernel to grow this process's heap by 'increment' bytes.
// Returns the start_address of the newly available memory, or (void*)-1
// on failure. This is the only thing in this file that talks to the 
// kernel - everything else is bookkeeping in user space.
static void* heap_grow(long increment)
{
    return (void*)__syscall1(SYS_HEAP_GROW, increment);
}

// Minimum amount to request from the kernel at once. Asking for exactly
// what's needed every time would mean a syscall (and at least one new
// physical page) per malloc() call - requesting a bit extra up front and
// keeping the leftover as a free block avoids that for the common case
// lots of small allocations.
#define MALLOC_CHUNK_SIZE 4096

// If 'block' is significantly bigger than 'size', carve the leftover
// off into a new free block and splice it into the list right after
// 'block'. Otherwise leave it alone - a sliver too small to hold even a
// header of its own isn't worth splitting it off.
static void split_block(block_header_t* block, size_t size)
{
    if(block->size < size + sizeof(block_header_t) + 1)
    {
        return;
    }

    size_t remaining = block->size - sizeof(block_header_t);

    block_header_t* new_block = (block_header_t*)((uint8_t*)(block + 1) + size);
    new_block->size = remaining;
    new_block->free = 1;
    new_block->next = block->next;

    block->size = size;
    block->next = new_block;
}

void* malloc(size_t size)
{
    if(size == 0)
    {
        return 0;
    }

    // Round up to a multiple of 8 so every block stays pointer-aligned
    // (8 bytes is all we need - SSE/MMX are disabled for user programs,
    // so there's no 16-byte alignment requirement to satisfy)
    size = (size + 7) & ~(size_t)7;

    // First fit search through existing blocks
    block_header_t* prev = 0;
    block_header_t* current = heap_head;

    while(current)
    {
        if(current->free && current->size >= size)
        {
            split_block(current, size);
            current->free = 0;
            return (void*)(current + 1);
        }

        prev = current;
        current = current->next;
    }

    // No free block was big enough - ask the kernel for more memory.
    // prev is now the last block in the list (or NULL if the heap is
    // still completley empty), so the new block goes right after it.
    size_t needed = size + sizeof(block_header_t);
    size_t request = needed > MALLOC_CHUNK_SIZE ? needed : MALLOC_CHUNK_SIZE;

    void* region = heap_grow((long)request);

    if(region == (void*)-1)
    {
        return 0;
    }

    block_header_t* block = (block_header_t*)region;
    block->size = request - sizeof(block_header_t);
    block->free = 0;
    block->next = 0;

    if(prev)
    {
        prev->next = block;
    }
    else
    {
        heap_head = block;
    }

    // If we asked for a whole chunk but only needed part of it, give
    // the rest back as a free block for next time
    split_block(block, size);

    return (void*)(block + 1);
}

void free(void* ptr)
{
    if(!ptr)
    {
        return;
    }

    block_header_t* block = (block_header_t*)ptr - 1;
    block->free = 1;

    // Merge forwards with the next block if it's also free. Only forward
    // this is a singly linked list, so merging backward would need a
    // full walk from the head to find the previous block. Skipped for
    // now good enough for a basic allocator, and doesn't lose memory,
    // just leaves it as smaller adjacent free blocks instead of one
    // big one until something forces a walk past them.
    if(block->next && block->next->free)
    {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}