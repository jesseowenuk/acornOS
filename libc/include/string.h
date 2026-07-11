#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
char* strcpy(char* dst, const char* src);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
void* memset(void* dst, int value, size_t count);
void* memcpy(void* dst, const void* src, size_t count);
int memcmp(const void* a, const void* b, size_t count);

#endif