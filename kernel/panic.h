#ifndef PANIC_H
#define PANIC_H

#include "kprintf.h"
#include "vga.h"

// Kernel panic - print a message and halt forever
// Called when the kernel encounters an unrecoverable error
void kpanic(const char* msg);

// Assertion macro - panics if condition is false
#define KASSERT(cond, msg) \
    do { \
        if(!(cond)) { \
            kpanic("ASSERT FAILED: " msg); \
        } \
    } while(0)

#endif