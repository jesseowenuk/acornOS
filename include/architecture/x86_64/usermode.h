#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

struct process;

// Jump to user mode and execute the given function
// This function never returns - the user program must call SYS_EXIT
// entry = address of function to run in ring 3
// stack = top of the user mode stack
void enter_usermode(uint64_t entry, uint64_t stack);
void jump_to_usermode(void (*entry)());

// Return to usermode using iret frame
void iret_to_usermode();

// Restore proc->cpu general registers and iret into ring 3 using the
// frame proc->cpu.rsp already points at (built by elf_load()). Never returns
void enter_ring3(struct process* proc);

#endif