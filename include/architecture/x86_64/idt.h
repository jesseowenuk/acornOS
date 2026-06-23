#ifndef IDH_H
#define IDH_H

#include <kernel/interrupts.h>

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t base_low;          // Lower 16 bits of handler address
    uint16_t selector;          // Kernel code segment selector
    uint8_t ist;                // Interupt Stack Table offset (0 = none)
    uint8_t flags;              // Type and attributes
    uint16_t base_mid;          // Bits 16-31 of handler address
    uint32_t base_high;         // Bits 32-63 of handler address
    uint32_t reserved;          // Always zero
} idt_entry_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint64_t base;
} idt_descriptor_t;

void idt_init();
void idt_set_entry(int n, uint64_t base, uint16_t selector, uint8_t flags);
void keyboard_handler(registers_t* regs);
void isr_handler(registers_t* regs);
void irq_handler(registers_t* regs);

#endif