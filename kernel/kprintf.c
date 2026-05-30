#include "kprintf.h"
#include "vga.h"                // for vga_putchar
#include "serial.h"             // For serial_putchar
#include <stdarg.h>             // For va_list etc.

// --- Integer string helpers --------------------------------

// Write an unsigned decimal number into buf
// Returns number of characters written
static int uint_to_dec(char* buf, uint32_t n)
{
    if(n == 0)
    {
        // Special case - zero
        buf[0] = '0';
        return 1;
    }

    // Temporary buffer - digits are in reverse
    char tmp[10];
    int i = 0;

    while(n > 0)
    {
        // Extract the last digit
        tmp[i++] = '0' + (n % 10);

        // Remove the last digit
        n /= 10;
    }

    // Reverse into buf
    int len = i;
    for(int j = 0; j < len; j++)
    {
        // Copy in reverse order
        buf[j] = tmp[len - 1 - j];
    }

    // Return number of digits written
    return len;
}

// Write a signed decimal number info buf
static int int_to_dec(char* buf, int32_t n)
{
    if(n < 0)
    {
        // Write minus sign
        buf[0] = '-';

        // Write absolute value after the minus sign
        return 1 + uint_to_dec(buf + 1, (uint32_t)(-n));
    }

    return uint_to_dec(buf, (uint32_t)n);
}

// Write a hex number into buf
// always writes 8 digits with leading zeros
static int uint_to_hex(char* buf, uint32_t n)
{
    const char* digits = "0123456789ABCDEF";

    for(int i = 7; i >= 0; i--)
    {
        // Extract lowest nibble
        buf[i] = digits[n & 0xF];

        // Shift to the right 4 bits
        n >>= 4;
    }

    // Always return 8 characters
    return 8;
}

// --- kvsnprintf ---------------------------------------------
// Core formatting function
// Writes formatted output into buf up to size-1 characters
// Returns total number of characters that would have been written

int kvsnprintf(char* buf, int size, const char* fmt, va_list args)
{
    // Current position in buf
    int pos = 0;

    // Helper lambda - write a single char to buf if space available
    // we define this as a macro since C doesn't have lambdas
    #define PUTC(c) do { \
        if(pos < size - 1) { \
            buf[pos] = (c); \
        } \
        pos++; \
    } while(0)

    // Helper - write a string to buf
    #define PUTS(s) do { \
        const char* _s = (s); \
        while(*_s) { PUTC(*_s++); } \
    } while(0)

    while(*fmt)
    {
        if(*fmt != '%')
        {
            // Normal character - copy directly
            PUTC(*fmt++);
            continue;
        }

        // Skip the '%'
        fmt++;

        // Handle format specifier
        switch(*fmt++)
        {
            // Signed decimal integer
            case 'd':
            {
                int32_t n = va_arg(args, int32_t);

                // Max 11 characters for 32-bit signed + null
                char tmp[12];

                int len = int_to_dec(tmp, n);

                for(int i = 0; i < len; i++)
                {
                    PUTC(tmp[i]);
                }
                break;
            }

            // Unsigned decimal integer
            case 'u':
            {
                uint32_t n = va_arg(args, uint32_t);

                // Max 10 characters for 32-bit unsigned + null
                char tmp[11];
                
                int len = uint_to_dec(tmp, n);

                for(int i = 0; i < len; i++)
                {
                    PUTC(tmp[i]);
                }
                break;
            }

            // Hexadecimal
            case 'x':
            {
                uint32_t n = va_arg(args, uint32_t);

                // "0x" + 8 hex digits + null
                char tmp[11];

                int len = uint_to_hex(tmp, n);

                for(int i = 0; i < len; i++)
                {
                    PUTC(tmp[i]);
                }
                break;
            }

            // String
            case 's':
            {
                const char* s = va_arg(args, const char*);

                if(!s)
                {
                    // Handle pointer gracefully
                    s = "(null)";
                }

                PUTS(s);
                break;
            }

            // Character
            case 'c':
            {
                // char is promoted to int in va_arg
                char c = (char)va_arg(args, int);

                PUTC(c);
                break;
            }

            // Literal percent
            case '%':
            {
                PUTC('%');
                break;
            }

            default:
            {
                // Unknown specifier - print as is
                PUTC('%');

                // Print the unknown specifier char
                PUTC(*(fmt - 1));

                break;
            }
        }
    }

    #undef PUTC
    #undef PUTS

    if(pos < size)
    {
        // Null terminate
        buf[pos] = 0;
    }
    else if(size > 0)
    {
        // Ensure null termination even if truncated
        buf[size - 1] = 0;
    }

    // Return total number of characters written
    return pos;
}

// --- kprintf ---------------------------------------------
// Formats and prints to VGA

void kprintf(const char* fmt, ...)
{
    // 1KB buffer - enough for any kernel message
    char buf[1024];

    va_list args;

    // Iniitialise args to point after fmt
    va_start(args, fmt);

    // Format into buffer
    kvsnprintf(buf, sizeof(buf), fmt, args);

    // Clean upva_list
    va_end(args);

    // Print buffer to VGA character by character
    const char* p = buf;

    while(*p)
    {
        vga_putchar(*p++);
    }
}

// Formats and prints to VGA

void kserial_printf(const char* fmt, ...)
{
    // 1KB buffer - enough for any kernel message
    char buf[1024];

    va_list args;

    // Iniitialise args to point after fmt
    va_start(args, fmt);

    // Format into buffer
    kvsnprintf(buf, sizeof(buf), fmt, args);

    // Clean upva_list
    va_end(args);

    // Print buffer to VGA character by character
    const char* p = buf;

    while(*p)
    {
        if(*p == '\n')
        {
            // Serial needs \r\n not just \n
            serial_putchar('\r');
        }

        serial_putchar(*p++);
    }
}