#ifndef KPRINTF_H
#define KPRINTF_H

// For va_list, va_start, va_arg, va_end
// This is compiler-provided so available in 
// freestanding mode
#include <stdarg.h>

// Print formatted string to VGA
// Supports: %d, %u, %x, %s, %c, %%
void kprintf(const char* fmt, ...);

// Print formatted string to serial port
void kserial_printf(const char* fmt, ...);

// Core formatter - writes to a buffer
// Used internally by kprintf and kserial_printf
// Returns mumber of characters written
int kvsnprintf(char* buf, int size, const char* fmt, va_list args);

#endif