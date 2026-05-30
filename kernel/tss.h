#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// --- Task State Segment -----------------------------------------------
// 104 byte structure required by the CPU for ring transitions
// We only use esp0 and ss0 - everything else is legacy hardware task switching
// __attribute__((packed)) ensures no padding between fields
typedef struct __attribute__((packed))
{
    uint32_t prev_tss;              // Previous TSS link - unused, always 0
    uint32_t esp0;                  // Kernel stack pointer - CPU loads this on ring 3->0
    uint32_t ss0;                   // Kernel stack segment - 0x10 (kernel data)
    uint32_t esp1;                  // Ring 1 stack - unused
    uint32_t ss1;                   // Ring 1 stack segment - unused
    uint32_t esp2;                  // Ring 2 stack - unused
    uint32_t ss2;                   // Ring 2 stack segment - unused
    uint32_t cr3;                   // Page directory - unused (we manage paging ourselves)
    uint32_t eip;                   // Instruction pointer - unused
    uint32_t eflags;                // CPU flags - unused
    uint32_t eax;                   // General purpose registers - all unused
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;                    // Segment registers - all unused
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;                   // LDT selector - unused
    uint16_t trap;                  // Debug trap flag - unused
    uint16_t iobp;                  // I/O permission bitmap offset
                                    // Set to sizeof(tss_t) to disable I/O from ring 3
} tss_t;

// --- Functions ------------------------------------------------------------

// Initialise TSS and install into GDT
void tss_init();

// Update esp0 - called on every context switch
// so the CPU always knows the current kernel stack
void tss_set_kernel_stack(uint32_t stack);

#endif