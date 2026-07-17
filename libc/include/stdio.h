#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

// Minimal printf - supports %d, %u, %x, %c, %s, %% and their 'l' (long) variants
// Formats into an internal buffer then issues a single write()
int printf(const char* fmt, ...);
int vprintf(const char* fmt, va_list args);

// --- Buffered file I/O ----------------------------------------------------------
// A FILE wraps a raw fd (see uinstd.h) with an internal buffer so callers
// doing lots of small reads/writes don't issue a syscall for every one.

typedef struct FILE FILE;

// Open 'path' with a buffered FILE. 'mode' supports "r" (read, must
// already exist), "w" (write, create/truncate) and "a" (write, create,
// append). Returns NULL on failure
FILE* fopen(const char* path, const char* mode);

// Flush any buffered writes, close the underlying fd and free the FILE.
// Returns 0 on success, -1 on failure.
int fclose(FILE* f);

// Read up to nmemb items of 'size' bytes each into ptr. Returns the
// mumber of whole items actually read (less than nmemb at EOF or error).
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f);

// Buffer up to nmemb of 'size' bytes each from ptr, flushing to
// the underling fd as the buffer fills. Returns the number of whole
// items actually accepted (less than nmemb only on a write error).
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f);

#endif