// Generic interrupt interface - architecture independent

#ifndef KERNEL_INTERUPTS_H
#define KERNEL_INTERUPTS_H

#include <stdint.h>

// CPU register state at time of interrupt
// Architecture specific but named generically.
typedef struct
{
    uint64_t ds;
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi;
    uint64_t rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

// Generic interrupt interface

// Initialise interrupt handling (arch implements)
void interrupts_init();   
void idt_init();

void idt_set_entry(int n, uint64_t base, uint16_t selector, uint8_t flags);

// Enable interrupts
void interrupts_enable();

// Disable interrupts
void interrupts_disable();

// Interrupt handlers - called from arch-specific stubs

void isr_handler(registers_t* regs);
void irq_handler(registers_t* regs);

#endif