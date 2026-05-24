#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include "serial.h"
#include "vga.h"

// Print a decimal number to VGA
static inline void print_num_serial(uint32_t n)
{
    if(n == 0)
    {
        serial_putchar('0');
        return;
    }

    char buf[10];
    int i = 0;
    while(n > 0)
    {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while(i > 0)
    {
        serial_putchar(buf[--i]);
    }
}

// Print a hex number to serial
static inline void print_hex_serial(uint32_t n)
{
    const char* digits = "0123456789ABCDEF";
    serial_print("0x");
    char buf[8];
    for(int i = 7; i >= 0; i--)
    {
        buf[i] = digits[n & 0xF];
        n >>= 4;
    }

    for(int i = 0; i < 8; i++)
    {
        serial_putchar(buf[i]);
    }
}

#endif