#ifndef KERNEL_TSS_H
#define KERNEL_TSS_H

#include <stdint.h>

// --- Interface ------------------------------------------------------------

// Initialise TSS and install into GDT
void tss_init();

// Update esp0 - called on every context switch
// so the CPU always knows the current kernel stack
void tss_set_kernel_stack(uint64_t stack);

#endif