#ifndef KEYBOARD_H
#define KEYBOARD_H

// Forward declaration - avoids circular include with process.h
struct process;

// Initialise the keyboard driver
void keyboard_init();

// The process currently waiting for keyboard input
// NULL = nobody waiting
extern struct process* keyboard_waiting;

// Block the current process until a key is pressed
void keyboard_wait(struct process* proc);

// Block until a key is available
char keyboard_getchar();

#endif