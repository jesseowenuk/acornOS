#include <string.h>

size_t strlen(const char* str)
{
    size_t len = 0;

    while(str[len])
    {
        len++;
    }

    return len;
}

char* strcpy(char* dst, const char* src)
{
    char* result = dst;

    while((*dst++ = *src++)) 
    {

    }

    return result;
}

int strcmp(const char* a, const char* b)
{
    while(*a && (*a == *b))
    {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    while(n > 0 && *a && (*a == *b))
    {
        a++;
        b++;
        n--;
    }

    if(n == 0)
    {
        return 0;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

void* memset(void* dst, int value, size_t count)
{
    unsigned char* d = (unsigned char*)dst;

    for(size_t i = 0; i < count; i++)
    {
        d[i] = (unsigned char)value;
    }

    return dst;
}

void* memcpy(void* dst, const void* src, size_t count)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;

    for(size_t i = 0; i < count; i++)
    {
        d[i] = s[i];
    }

    return dst;
}

int memcmp(const void* a, const void* b, size_t count)
{
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;

    for(size_t i = 0; i < count; i++)
    {
        if(pa[i] != pb[i])
        {
            return pa[i] - pb[i];
        }
    }

    return 0;
}