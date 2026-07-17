#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// --- Buffered file I/O ----------------------------------------------
//
// Every read()/write() is a syscall, and every syscall is a full ring 3 to
// ring 0 SYSCALL/SYSRET round trip - not free. A FILE* sits in front of a
// raw fd with one internal buffer:
//
//  read mode:  a read-ahead cache. fread() serves bytes out of the buffer
//              and only calls read() again once the buffer is drained,
//              so many small fread() calls can share one syscall's worth
//              of data.
//
//  write mode: a write-behind cache. fwrite() copies into the buffer and
//              only calls write() once the buffer is full (of fclose()
//              flushes whatever's left), so many small fwrite() calls
//              are coalesced into one syscall.
//
// A single FILE is only ever used in one direction, so buf_pos/buf_len
// have mode-dependent meaning (documented on the struct below) rather
// than keeping seperate read and write state that would never both be
// in use at once.

#define FILE_BUF_SIZE 512

struct FILE
{
    int fd;
    int is_write;                   // 1 = write/append mode, 0 = read mode
    char* buf;
    size_t buf_size;                
    size_t buf_pos;                 // read: index of next unread byte in buf
                                    // write: number of bytes currently buffered 
    size_t buf_len;                 // read only: valid bytes in buf since the last refill
};

FILE* fopen(const char* path, const char* mode)
{
    int flags;
    int is_write;

    switch(mode[0])
    {
        case 'r':
        {
            flags = O_RDONLY;
            is_write = 0;
            break;
        }

        case 'w':
        {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            is_write = 1;
            break;
        }

        case 'a':
        {
            flags = O_WRONLY | O_CREAT | O_APPEND;
            is_write = 1;
            break;
        }

        default:
        {
            return NULL;
        }
    }

    int fd = open(path, flags);

    if(fd < 0)
    {
        return NULL;
    }

    FILE* f = malloc(sizeof(FILE));

    if(!f)
    {
        close(fd);
        return NULL;
    }

    f->buf = malloc(FILE_BUF_SIZE);

    if(!f->buf)
    {
        free(f);
        close(fd);
        return NULL;
    }

    f->fd = fd;
    f->is_write = is_write;
    f->buf_size = FILE_BUF_SIZE;
    f->buf_pos = 0;
    f->buf_len = 0;

    return f;
}

int fclose(FILE* f)
{
    int result = 0;

    if(f->is_write && f->buf_pos > 0)
    {
        if(write(f->fd, f->buf, f->buf_pos) < 0)
        {
            result = -1;
        }
    }

    if(close(f->fd) < 0)
    {
        result = -1;
    }

    free(f->buf);
    free(f);

    return result;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* f)
{
    if(f->is_write || size == 0 || nmemb == 0)
    {
        return 0;
    }

    unsigned char* dst = ptr;
    size_t total = size * nmemb;
    size_t copied = 0;

    while(copied < total)
    {
        if(f->buf_pos >= f->buf_len)
        {
            ssize_t n = read(f->fd, f->buf, f->buf_size);

            if(n <= 0)
            {
                break;
            }

            f->buf_len = (size_t)n;
            f->buf_pos = 0;
        }

        size_t available = f->buf_len - f->buf_pos;
        size_t remaining = total - copied;
        size_t chunk = available < remaining ? available : remaining;

        memcpy(dst + copied, f->buf + f->buf_pos, chunk);
        f->buf_pos+= chunk;
        copied += chunk;
    }

    return copied / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* f)
{
    if(!f->is_write || size == 0 || nmemb == 0)
    {
        return 0;
    }

    const unsigned char* src = ptr;
    size_t total = size * nmemb;
    size_t written = 0;

    while(written < total)
    {
        if(f->buf_pos >= f->buf_size)
        {
            if(write(f->fd, f->buf, f->buf_pos) < 0)
            {
                break;
            }

            f->buf_pos = 0;
        }

        size_t space = f->buf_size - f->buf_pos;
        size_t remaining = total - written;
        size_t chunk = space < remaining ? space : remaining;

        memcpy(f->buf + f->buf_pos, src + written, chunk);
        f->buf_pos += chunk;
        written += chunk;
    }

    return written / size;
}