#include "idt.h"
#include "vga.h"
#include "pic.h"
#include "keyboard.h"
#include "timer.h"
#include "paging.h"
#include "serial.h"
#include "scheduler.h"
#include "syscall.h"
#include "kprintf.h"

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
extern void isr14();
extern void isr128();

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
    idt_set_entry(14, (uint32_t)isr14, 0x08, 0x8E);     // Page fault
    idt_set_entry(32, (uint32_t)irq0, 0x08, 0x8E);      // Timer
    idt_set_entry(33, (uint32_t)irq1, 0x08, 0x8E);      // Keyboard
    idt_set_entry(128, (uint32_t)isr128, 0x08, 0xEE);   // 0xEE = present, ring 3 callable, interrupt gate. Ring 3 callable 
                                                        // means user programs can trigger it without a GPF

    idt_flush((uint32_t)&descriptor);
}

// Called from isr_common_stub in isr.asm
void isr_handler(registers_t* regs)
{
    if(regs->int_no == 14)
    {
        // 14 = page fault
        // Forward to paging module
        page_fault_handler(regs);
        return;
    }

    // System call (INT 0x80)
    if(regs->int_no == 128)
    {
        syscall_handler(regs);
        return;
    }

    const char* exceptions[] = {
        "Divide By Zero",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint"
    };

    vga_set_colour(RED, BLACK);
    kprintf("\nCPU EXCEPTION: %s\n", regs->int_no < 4 ? exceptions[regs->int_no] : "Unknown");

    for(;;);
}

void irq_handler(registers_t* regs)
{
    // 32 = IRQ0 = timer
    if(regs->int_no == 32)              // IRQ0 = timer
    {
        timer_handler(regs);            // Update clock
        scheduler_tick(regs);           // Run scheduler - may switch process
    }

    // 33 = IRQ1 = kayboard
    if(regs->int_no == 33)
    {
        // Forward to the keyboard handler
        keyboard_handler(regs);
    }

    // Always send End of Interrupt to PIC so it knows we're done
    // and can send the next interrupt
    pic_send_eoi(regs->int_no - 32);
}