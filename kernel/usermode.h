#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

// Jump to user mode and execute the given function
// This function never returns - the user program must call SYS_EXIT
// entry = address of function to run in ring 3
// stack = top of the user mode stack
void enter_usermode(uint64_t entry, uint64_t stack);
void jump_to_usermode(void (*entry)());

void iret_to_usermode();

#endif