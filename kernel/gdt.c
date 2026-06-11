#include "gdt.h"
#include "kprintf.h"

#define GDT_ENTRIES 6

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

    // User mode segments - ring 3
    // Access byte 0xFA = 11111010b
    //  bit 7: present (1)
    //  bits 5-6: DPL = ring 3 (11)
    //  bit 4: code/data descriptor (1)
    //  bit 3: code segment (1)
    //  bit 1: readable (1)
    set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);        // User code - ring 3

    // Access byte 0xF2 = 11110010b
    //  bit 7: present (1)
    //  bits 5-6: DPL = ring 3 (11)
    //  bit 4: code/data descriptor (1)
    //  bit 3: data segment (0)
    //  bit 1: writable (1)
    set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);        // User data - ring 3

    // Entry 5 reserved for TSS - set up in tss_init()
    set_entry(5, 0, 0, 0, 0);                       // TSS placeholder - filled later

    descriptor.limit = sizeof(gdt) - 1;             // Size of GDT minus 1
    descriptor.base = (uint64_t)&gdt;               // Address of GDT

    gdt_flush((uint64_t)&descriptor);               // Load into CPU and reload segments
}

// --- TSS entry ---------------------------------------------------
// Called by tss_init() to install the TSS descriptor into the GDT
// The TSS descriptor has a special format different from code/data entries.
void gdt_set_tss_entry(uint32_t base, uint32_t limit)
{
    // TSS descriptor access byte = 0x89 = 10001001b
    //  bit 7: present (1)
    //  bits 5-6: DPL = ring 0 (00)
    //  bit 4: must be 0 for system descriptor
    //  bits 0-3: type 1001 = 32-bit TSS available
    gdt[5].limit_low        = limit & 0xFFFF;
    gdt[5].base_low         = base & 0xFFFF;
    gdt[5].base_mid         = (base >> 16) & 0xFF;
    gdt[5].access           = 0x89;         // Present, ring 0, TSS type
    gdt[5].granularity      = (limit >> 16) & 0x0F;
    gdt[5].base_high        = (base >> 24) & 0xFF;

    // Reload the GDT with the new TSS entry
    gdt_flush((uint64_t)&descriptor);
}