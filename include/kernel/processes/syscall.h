#ifndef SYSCALL_H
#define SYSCALL_H

#include <kernel/interrupts.h>

// --- Syscall numbers ---------------------------------------------
#define SYS_EXIT    0               // Terminate current process
#define SYS_WRITE   1               // Write string to screen
#define SYS_READ    2               // Read character from keyboard
#define SYS_GETPID  3               // Get current process ID
#define SYS_YIELD   4               // Voluntarily yield the CPU
#define SYS_FORK    5               // Fork current process
#define SYS_WAIT    6               // Wait for child to exit
#define SYS_EXEC    7               // Replace process image
#define SYS_OPEN    8
#define SYS_SEEK    9
#define SYS_MKDIR   10
#define SYS_READDIR 11
#define SYS_DELETE  12

// --- Syscall handler ---------------------------------------------
// Called from isr_handler when INT 0x80 fires
// regs->rax    = syscall number
// regs->rbx    = first argument
// regs->rcx    = second argument
// regs->rdx    = third argument
// return value goes into regs->rax
void syscall_handler(registers_t* regs);

// Register INT 0x80 handler
void syscall_init();

#endif