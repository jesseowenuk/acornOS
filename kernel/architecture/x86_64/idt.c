#include <architecture/x86_64/idt.h>
#include <architecture/x86_64/paging.h>
#include <architecture/x86_64/pic.h>
#include <architecture/x86_64/tss.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <kernel/core/kprintf.h>
#include <kernel/processes/scheduler.h>
#include <kernel/processes/syscall.h>

#include <stdint.h>

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_descriptor_t descriptor;

extern void idt_flush(uint64_t);
extern void syscall_entry();

// ISR stubs declared in isr.asm
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr8();
extern void isr13();
extern void isr14();

extern void irq0();
extern void irq1();

void idt_set_entry(int n, uint64_t base, uint16_t selector, uint8_t flags)
{
    idt[n].base_low = base & 0xFFFF;
    idt[n].base_mid = (base >> 16) & 0xFFFF;
    idt[n].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[n].selector = selector;
    idt[n].ist = 0;
    idt[n].flags = flags;
    idt[n].reserved = 0;
}

void idt_init()
{
    descriptor.limit = sizeof(idt) - 1;
    descriptor.base = (uint64_t)&idt;

    // CPU exception handlers (first 4 for now)
    idt_set_entry(0, (uint64_t)isr0, 0x08, 0x8E);       // Divide by zero
    idt_set_entry(1, (uint64_t)isr1, 0x08, 0x8E);       // Debug
    idt_set_entry(2, (uint64_t)isr2, 0x08, 0x8E);       // Non-maskable interrupt
    idt_set_entry(3, (uint64_t)isr3, 0x08, 0x8E);       // Breakpoint
    idt_set_entry(8, (uint64_t)isr8, 0x08, 0x8E);       // Double fault
    idt_set_entry(13, (uint64_t)isr13, 0x08, 0x8E);     // General protection fault
    idt_set_entry(14, (uint64_t)isr14, 0x08, 0x8E);     // Page fault
    idt_set_entry(32, (uint64_t)irq0, 0x08, 0x8E);      // Timer
    idt_set_entry(33, (uint64_t)irq1, 0x08, 0x8E);      // Keyboard

    idt_flush((uint64_t)&descriptor);
}

void syscall_msr_init()
{
    // Enable SYSCALL/SYSRET via EFER bit
    uint32_t efer_lo;
    uint32_t efer_hi;

    __asm__ volatile(
        "rdmsr"
        : "=a"(efer_lo), "=d"(efer_hi)
        : "c"(0xC0000080UL)
    );

    efer_lo |= 1;

    __asm__ volatile(
        "wrmsr"
        :
        : "a"(efer_lo), "d"(efer_hi), "c"(0xC0000080UL)
    );

    // STAR MSR - selectors used for SYSCALL/SYSRET
    // bits 47:32 = kernel CS (SYSCALL loads CS=this, SS=this+8)
    // bits 63:48 = base for SYSRET (CS=this+16|3, SS=this+8|3)
    uint64_t star = ((uint64_t)0x18 << 48) | ((uint64_t)0x08 << 32);
    __asm__ volatile(
        "wrmsr"
        :
        : "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(0xC0000081UL)
    );

    // LSTAR MSR - syscall entry point
    uint64_t lstar = (uint64_t)syscall_entry;
    __asm__ volatile(
        "wrmsr"
        :
        : "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(0xC0000082UL)
    );

    // FMASK - clear IF on syscall entry
    uint64_t fmask = 0x200UL;
    __asm__ volatile(
        "wrmsr"
        :
        : "a"((uint32_t)fmask), "d"((uint32_t)(fmask >> 32)), "c"(0xC0000084UL)
    );
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

    if(regs->int_no == 14)
    {
        page_fault_handler(regs);
        return;
    }

    if(regs->int_no == 8)
    {
        vga_set_colour(RED, BLACK);
        kprintf("\nCPU Exception: Double Fault\n");
        kprintf("EIP=0x%lx ESP=0%lx\n", regs->rip, regs->rsp);
        for(;;);
    }

    if(regs->int_no == 13)
    {
        vga_set_colour(RED, BLACK);
        kprintf("\nCPU Exception: General Protection Fault\n");
        kprintf("EIP=0x%lx ESP=0%lx\n", regs->rip, regs->rsp);
        for(;;);
    }

    const char* exceptions[] = {
        "Divide By Zero",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint"
    };

    vga_set_colour(RED, BLACK);
    kprintf("\nCPU EXCEPTION: %s\n", exceptions[regs->int_no]);
    kprintf("EIP=0x%x ESP=0x%x\n", regs->rip, regs->rsp);

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