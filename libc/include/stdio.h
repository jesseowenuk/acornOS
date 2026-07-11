#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

// Minimal printf - supports %d, %u, %x, %c, %s, %% and their 'l' (long) variants
// Formats into an internal buffer then issues a single write()
int printf(const char* fmt, ...);
int vprintf(const char* fmt, va_list args);

#endif