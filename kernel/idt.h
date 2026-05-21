#ifndef IDH_H
#define IDH_H

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t base_low;          // Lower 16 bits of handler address
    uint16_t selector;          // Kernel code segment selector
    uint8_t zero;               // Always zero
    uint8_t flags;              // Type and attributes
    uint16_t base_high;         // Upper 16 bits of handler address
} idt_entry_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint32_t base;
} idt_descriptor_t;

// Registers pushed by our ISR stubs
typedef struct
{
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

void idt_init();
void idt_set_entry(int n, uint32_t base, uint16_t selector, uint8_t flags);
void keyboard_handler(registers_t* regs);
void isr_handler(registers_t* regs);
void irq_handler(registers_t* regs);

#endif