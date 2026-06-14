#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// --- Task State Segment -----------------------------------------------
// In 64-bit mode the TSS is much simpler than in 32-bit
// Legacy fields are gone - only stack pointers and I/O bitmap remain
typedef struct __attribute__((packed))
{
    uint32_t reserved0;             // reserved
    uint64_t rsp0;                  // kernel stack pointer for ring 0
    uint64_t rsp1;                  // stack for ring 1 (unused)
    uint64_t rsp2;                  // stack for ring 2 (unused)
    uint64_t reserved1;             // reserved
    uint64_t ist1;                  // Interrupt Stack Table entries (unused)
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;             // reserved
    uint16_t reserved3;             // reserved
    uint16_t iobp;                  // I/O permission bitmap offset
} tss_t;

// --- Functions ------------------------------------------------------------

// Initialise TSS and install into GDT
void tss_init();

// Update esp0 - called on every context switch
// so the CPU always knows the current kernel stack
void tss_set_kernel_stack(uint64_t stack);

#endif