#include <architecture/x86_64/gdt.h>
#include <kernel/core/kprintf.h>

// In 64-bit mode the TSS descriptor is 16 bytes (two slots)
// So we need 7 entries, null, kcode, kdata, ucode, udata, tss_low, tss_high
#define GDT_ENTRIES 8

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_descriptor_t descriptor;

// Declared in gdt_flush.asm - reloads segment registers
extern void gdt_flush(uint64_t);

static void set_entry(int i, uint8_t access, uint8_t granularity)
{
    // In 64-bit mode base and limit are ignored for code/data
    gdt[i].base_low = 0;                            // Bits of 0-15 of base
    gdt[i].base_mid = 0;                            // Bits 16-23 of base
    gdt[i].base_high = 0;                           // Bits 24-31 of base
    gdt[i].limit_low = 0xFFFF;                      // Bits 0-15 of limit
    gdt[i].granularity = granularity;               // Bits 16-19 of limit
    gdt[i].access = access;                         // The access byte
}

void gdt_init()
{
    // 0: Null descriptor (required)
    set_entry(0, 0x00, 0x00);       

    // 1: Kernel code (64-bit)
    // Access: 0x9A = present, ring 0, code, executable, readable
    // Granularity: 0x20 = L=1 (64-bit), D=0 (must be 0 in 64-bit) 
    set_entry(1, 0x9A, 0x20);

    // 2: Kernel data (64-bit)
    // Access: 0x92 = present, ring 0, data, writable
    // Granularity: 0x00 (data segments ignore L bit)
    set_entry(2, 0x92, 0x00);  
    
    // 3: Placeholder (unused) - never loaded into a segment register
    // Exists so SYSRET's fixed CS=base+16 / SS=base+8 math lands on the
    // right descriptors below. See syscall_msr_init() STAR MSR setup
    set_entry(3, 0x00, 0x00);

    // 4: User Data (ring 3)
    // Access: 0xF2 = present, ring 3, data, wriable
    set_entry(4, 0xF2, 0x00);

    // 5: User code (64-bit, ring 3)
    // Access: 0xFA = present, ring 3, data, writable
    set_entry(5, 0xFA, 0x20);

    // 6 & 7: TSS - 16-byte descriptor in 64-bit mode (filled by tss_init)
    gdt[6].base_low = 0;
    gdt[6].base_mid = 0;
    gdt[6].base_high = 0;
    gdt[6].limit_low = 0;
    gdt[6].granularity = 0;
    gdt[6].access = 0;

    // Entry 7 = upper 8 bytes of TSS descriptor
    // Treated as a raw uint64_t by tss_init
    gdt[7].base_low = 0;
    gdt[7].base_mid = 0;
    gdt[7].base_high = 0;
    gdt[7].limit_low = 0;
    gdt[7].granularity = 0;
    gdt[7].access = 0;

    descriptor.limit = sizeof(gdt) - 1;             // Size of GDT minus 1
    descriptor.base = (uint64_t)&gdt;               // Address of GDT

    kserial_printf("GDT: descriptor at 0x%lx\n", (uint64_t)&descriptor);
    kserial_printf("GDT: base=0x%lx limit=%d\n", descriptor.base, descriptor.limit);

    gdt_flush((uint64_t)&descriptor);               // Load into CPU and reload segments
}

// --- TSS entry ---------------------------------------------------
// Called by tss_init() to install the TSS descriptor into the GDT
// The TSS descriptor has a special format different from code/data entries.
void gdt_set_tss_entry(uint64_t base, uint64_t limit)
{
    // TSS descriptor is 16 bytes spread across two GDT entries
    // Entry 5: lower 8 bytes
    gdt[6].limit_low        = limit & 0xFFFF;
    gdt[6].base_low         = base & 0xFFFF;
    gdt[6].base_mid         = (base >> 16) & 0xFF;
    gdt[6].access           = 0x89;         // Present, ring 0, TSS type
    gdt[6].granularity      = (limit >> 16) & 0x0F;
    gdt[6].base_high        = (base >> 24) & 0xFF;

    // Entry 6: upper8 bytes (base bits 63:32 + reserved)
    // Reinterpret as uint64_t for simplicity
    uint64_t* tss_high = (uint64_t*)&gdt[7];
    *tss_high = (base >> 32) & 0xFFFFFFFF;

    // Reload the GDT with the new TSS entry
    gdt_flush((uint64_t)&descriptor);
}