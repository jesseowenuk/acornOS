#ifndef SHELL_H
#define SHELL_H

// Maximium number of characters in a single command
// We need a fixed size we have no dynamic memory yet
#define SHELL_BUFFER_SIZE 256

// Command table entry
typedef struct
{
    const char* name;
    void (*handler)(const char* args);
} command_t;

void shell_init();              // Print the prompt for the first time
void shell_handle_key(char c);  // Called by keyboard driver for each keypress   

#endif