#include "tss.h"
#include "gdt.h"            // For gdt_set_tss_entry, GDT_KERNEL_DATA
#include "kprintf.h"        // For kserial_printf

// --- TSS instance --------------------------------------------------
// One global TSS - we update esp0 on every process switch
static tss_t tss;

// --- tss_init ------------------------------------------------------
void tss_init()
{
    // Zero out the entire TSS first
    uint8_t* p = (uint8_t*)&tss;

    for(uint32_t i = 0; i < sizeof(tss_t); i++)
    {
        // Clear out every byte
        p[i] = 0;
    }

    // Set the fields we actually need

    // Kernel data segment = 0x10
    // CPU loads this as SS on ring 3->0
    tss.ss0 = GDT_KERNEL_DATA;

    // Initial kernel stack
    // Updated on every context switch
    // via tss_set_kernel_stack()
    tss.esp0 = 0x90000;

    // I/O permission bitmap offset
    // Setting to sizeof(tss_t) places it
    // beyond the TSS - disables all I/O
    // port access from ring 3
    tss.iobp = sizeof(tss_t);

    // Install TSS descriptor into GDT entry 5
    // base = address of our tss_struct
    // limit = size of TSS minus 1
    gdt_set_tss_entry(
        (uint64_t)&tss,                 // Physical address of TSS
        sizeof(tss_t) - 1               // Limit = size - 1 (CPU convention)
    );

    // Load the TSS selector into the TR (task Register)
    // 0x28 = GDT entry 5 offset (5 * 8 = 40 = 0x28)
    // The CPU reads TR to find the TSS on every ring transition
    __asm__ volatile(
        "ltr %%ax"                      // Load Task Register with TSS selector
        :                               // No output
        : "a"(0x28)                     // Input: TSS selector = 0x28
    );

    kserial_printf("TSS: initialised at 0x%x\n", (uint64_t)&tss);
    kserial_printf("TSS: esp=0x%x ss0=0x%x\n", tss.esp0, tss.ss0);
}

// --- tss_set_kernel_stack ---------------------------------------------
// Update the kernel stack pointer in the TSS
// Must be called on every context switch so the CPU knows which
// kernel stack to use when the new process triggers an interrupt

void tss_set_kernel_stack(uint32_t stack)
{
    // Update kernel stack pointer
    // CPU reads this automatically on
    // every ring 3 -> 0 transition
    tss.esp0 = stack;
}