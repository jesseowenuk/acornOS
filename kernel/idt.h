#ifndef IDH_H
#define IDH_H

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

// Registers pushed by our ISR stubs
typedef struct
{
    uint64_t ds;                    // Data segment
    uint64_t r15, r14, r13, r12; 
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi;
    uint64_t rdx, rcx, rbx, rax;

    // Pushed by our macros
    uint64_t int_no;
    uint64_t err_code;

    // Pushed by CPU automatically
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;               // User stack pointer
    uint64_t ss; 
} registers_t;

void idt_init();
void idt_set_entry(int n, uint64_t base, uint16_t selector, uint8_t flags);
void keyboard_handler(registers_t* regs);
void isr_handler(registers_t* regs);
void irq_handler(registers_t* regs);

#endif