#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"                // For registers_t

// --- Syscall numbers ---------------------------------------------
#define SYS_EXIT    0               // Terminate current process
#define SYS_WRITE   1               // Write string to screen
#define SYS_READ    2               // Read character from keyboard
#define SYS_GETPID  3               // Get current process ID
#define SYS_YIELD   4               // Voluntarily yield the CPU
#define SYS_FORK    5               // Fork current process
#define SYS_WAIT    6               // Wait for child to exit
#define SYS_EXEC    7               // Replace process image

// --- Syscall handler ---------------------------------------------
// Called from isr_handler when INT 0x80 fires
// regs->eax    = syscall number
// regs->ebx    = first argument
// regs->ecx    = second argument
// regs->edx    = third argument
// return value goes into regs->eax
void syscall_handler(registers_t* regs);

// Register INT 0x80 handler
void syscall_init();

#endif