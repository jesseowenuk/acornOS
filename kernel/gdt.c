#include "gdt.h"

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_descriptor_t descriptor;

// Declared in gdt_flush.asm - reloads segment registers
extern void gdt_flush(uint32_t);

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity)
{
    gdt[i].base_low = base & 0xFFFF;
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].granularity = (granularity & 0xF0) | ((limit >> 16) & 0x0F);
    gdt[i].access = access;
}

void gdt_init()
{
    set_entry(0, 0, 0x00000000, 0x00, 0x00);        // Null descriptor
    set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);        // Kernel code
    set_entry(2, 0, 0x00000000, 0x92, 0xCF);        // Kernel data

    descriptor.limit = sizeof(gdt) - 1;
    descriptor.base = (uint32_t)&gdt;

    gdt_flush((uint32_t)&descriptor);
}