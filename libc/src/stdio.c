#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

// --- Number formatting helpers ------------------------------------------

static int format_uint(char* buf, unsigned long value, int base)
{
    const char* digits = "0123456789abcdef";
    char tmp[32];
    int i = 0;

    if(value == 0)
    {
        tmp[i++] = '0';
    }

    while(value > 0)
    {
        tmp[i++] = digits[value % (unsigned long)base];
        value /= (unsigned long)base;
    }

    int len = i;

    while(i > 0)
    {
        *buf++ = tmp[--i];
    }

    return len;
}

static int format_int(char* buf, long value, int base)
{
    if(value < 0)
    {
        *buf++ = '-';
        return 1 + format_uint(buf, (unsigned long)(-value), base);
    }

    return format_uint(buf, (unsigned long)value, base);
}

// --- vprintf / printf -------------------------------------------------

int vprintf(const char* fmt, va_list args)
{
    char buf[1024];
    int pos = 0;

    for(const char* p = fmt; *p && pos < (int)sizeof(buf) - 32; p++)
    {
        if(*p != '%')
        {
            buf[pos++] = *p;
            continue;
        }

        p++;

        int is_long = 0;

        if(*p == 'l')
        {
            is_long = 1;
            p++;
        }

        switch(*p)
        {
            case 'd':
            {
                long value = is_long ? va_arg(args, long) : (long)va_arg(args, int);
                pos += format_int(buf + pos, value, 10);
                break;
            }

            case 'u':
            {
                unsigned long value = is_long ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
                pos += format_uint(buf + pos, value, 10);
                break;
            }

            case 'x':
            {
                unsigned long value = is_long ? va_arg(args, unsigned long) : (unsigned long)va_arg(args, unsigned int);
                pos += format_uint(buf + pos, value, 16);
                break;
            }

            case 'c':
            {
                buf[pos++] = (char)va_arg(args, int);
                break;
            }

            case 's':
            {
                const char* str = va_arg(args, const char*);

                while(*str && pos < (int)sizeof(buf) - 1)
                {
                    buf[pos++] = *str++;
                }

                break;
            }

            case '%':
            {
                buf[pos++] = '%';
                break;
            }

            default:
            {
                buf[pos++] = '%';
                buf[pos++] = *p;
                break;
            }
        }
    }

    write(STDOUT_FILENO, buf, (size_t)pos);

    return pos;
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = vprintf(fmt, args);
    va_end(args);

    return result;
}