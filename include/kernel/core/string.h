#ifndef STRING_H
#define STRING_H

#include <stdint.h>

// Copy src into dst up to max-1 characters, always NULL terminates
static inline void kstrcpy(char* dst, const char* src, int max)
{
    int i = 0;
    while(src[i] && i < max - 1)
    {
        // Copy until null or max reached
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// Compare two strings - returns 0 if equal, non-zero if different
static inline int kstrcmp(const char* a, const char* b)
{
    while(*a && *b)
    {
        if(*a != *b)
        {
            return *a - *b;
        }

        a++;
        b++;
    }

    return *a - *b;
}

// Returns 1 if strings are equal, 0 if not
static inline int kstreq(const char* a, const char* b)
{
    return kstrcmp(a, b) == 0;
}

// Returns length of string
static inline int kstrlen(const char* s)
{
    int i = 0;

    while(s[i])
    {
        i++;
    }

    return i;
}

// Copy n bytes from src to dst
static inline void kmemcpy(void* dst, const void* src, uint32_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    for(uint32_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }
}

// Set n bytes at dst to value
static inline void kmemset(void* dst, uint8_t value, uint32_t n)
{
    uint8_t* d = (uint8_t*)dst;

    for(uint32_t i = 0; i < n; i++)
    {
        d[i] = value;
    }
}

// Compare n bytes - return 0 if equal
static inline int kmemcmp(const void* a, const void* b, uint32_t n)
{
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;

    for(uint32_t i = 0; i < n; i++)
    {
        if(pa[i] != pb[i])
        {
            return pa[i] - pb[i];
        }
    }

    return 0;
}

#endif