#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "process.h"    // For process_t

// Initialise the keyboard driver
void keyboard_init();

// The process currently waiting for keyboard input
// NULL = nobody waiting
extern process_t* keyboard_waiting;

// Block the current process until a key is pressed
void keyboard_wait(process_t* proc);

// Block until a key is available
char keyboard_getchar();

#endif