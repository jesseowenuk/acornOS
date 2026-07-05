#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

#include <stdint.h>

// --- Flags ------------------------------------------------
#define GDT_KERNEL_CODE 0x08                        // Kernel code segment selector
#define GDT_KERNEL_DATA 0x10                        // Kernel data segment selector
#define GDT_USER_CODE   0x2B                        // User code segment (0x28 | RPL3)
#define GDT_USER_DATA   0x23                        // User data segment (0x20 | RPL3)
#define GDT_TSS         0x30                        // TSS selector

// --- Interface -------------------------------------------
void gdt_init();
void gdt_set_tss_entry(uint64_t base, uint64_t limit);

#endif