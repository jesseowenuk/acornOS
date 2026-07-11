#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

// Allocate 'size' bytes on the heap. Returns NULL on failure (out of
// memory) or if size is 0. Memory is NOT zeroed.
void* malloc(size_t size);

// Free memory previously returned by malloc(). Safe to call with NULL
// (does nothing)
void free(void* ptr);

#endif