#include "idt.h"
#include "vga.h"
#include "pic.h"

#include <stdint.h>

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_descriptor_t descriptor;

extern void idt_flush(uint32_t);

// ISR stubs declared in isr.asm
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();

extern void irq0();
extern void irq1();

void idt_set_entry(int n, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt[n].base_low = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].selector = selector;
    idt[n].zero = 0;
    idt[n].flags = flags;
}

void idt_init()
{
    descriptor.limit = sizeof(idt) - 1;
    descriptor.base = (uint32_t)&idt;

    // CPU exception handlers (first 4 for now)
    idt_set_entry(0, (uint32_t)isr0, 0x08, 0x8E);       // Divide by zero
    idt_set_entry(1, (uint32_t)isr1, 0x08, 0x8E);       // Debug
    idt_set_entry(2, (uint32_t)isr2, 0x08, 0x8E);       // Non-maskable interrupt
    idt_set_entry(3, (uint32_t)isr3, 0x08, 0x8E);       // Breakpoint
    idt_set_entry(32, (uint32_t)irq0, 0x08, 0x8E);      // Timer
    idt_set_entry(33, (uint32_t)irq1, 0x08, 0x8E);      // Keyboard

    idt_flush((uint32_t)&descriptor);
}

// Called from isr_common_stub in isr.asm
void isr_handler(registers_t regs)
{
    vga_set_colour(RED, BLACK);
    vga_print("\nCPU EXCEPTION: ");

    const char* exceptions[] = {
        "Divide By Zero",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint"
    };

    if(regs.int_no < 4)
    {
        vga_print(exceptions[regs.int_no]);
    }

    for(;;);
}

void irq_handler(registers_t regs)
{
    pic_send_eoi(regs.int_no - 32);
}