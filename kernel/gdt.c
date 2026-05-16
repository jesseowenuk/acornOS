#include "gdt.h"

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_descriptor_t descriptor;

// Declared in gdt_flush.asm - reloads segment registers
extern void gdt_flush(uint32_t);

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity)
{
    gdt[i].base_low = base & 0xFFFF;                // Bits of 0-15 of base
    gdt[i].base_mid = (base >> 16) & 0xFF;          // Bits 16-23 of base
    gdt[i].base_high = (base >> 24) & 0xFF;         // Bits 24-31 of base
    gdt[i].limit_low = (limit & 0xFFFF);            // Bits 0-15 of limit
    gdt[i].granularity = (limit >> 16) & 0xF0;      // Bits 16-19 of limit
                                                    // in the LOW nibble first
    gdt[i].granularity |= (granularity & 0xF0);     // Then OR in the flags
                                                    // into the HIGH nibble
    gdt[i].access = access;                         // The access byte
}

void gdt_init()
{
    set_entry(0, 0, 0x00000000, 0x00, 0x00);        // Null - all zero
    set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);        // Kernel code - 0xCF = 4KB granularity, 32-bit
    set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);        // Kernel data - same flags

    descriptor.limit = sizeof(gdt) - 1;             // Size of GDT minus 1
    descriptor.base = (uint32_t)&gdt;               // Address of GDT

    gdt_flush((uint32_t)&descriptor);               // Load into CPU and reload segments
}