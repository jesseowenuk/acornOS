#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08                        // Kernel code segment selector
#define GDT_KERNEL_DATA 0x10                        // Kernel data segment selector
#define GDT_USER_CODE   0x2B                        // User code segment (0x18 | RPL3)
#define GDT_USER_DATA   0x23                        // User data segment (0x20 | RPL3)
#define GDT_TSS         0x30                        // TSS selector

// One GDT entry (segment descriptor) - packed to prevent compiler padding
typedef struct __attribute__((packed))
{
    uint16_t limit_low;         // Lower 16 bits of segment limit
    uint16_t base_low;          // Lower 16 bits of base address
    uint8_t base_mid;           // Middle 8 bits of base address
    uint8_t access;             // Access flags (ring level, type etc)
    uint8_t granularity;        // Granularity + upper 4 bits of limit
    uint8_t base_high;          // Upper 8 bits of base address
} gdt_entry_t;

// Descriptor passed to lgdt instruction
typedef struct __attribute__((packed))
{
    uint16_t limit;             // Size of GDT minus 1
    uint64_t base;              // Address of first GDT entry
} gdt_descriptor_t;

void gdt_init();
void gdt_set_tss_entry(uint64_t base, uint64_t limit);

#endif