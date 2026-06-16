#include <drivers/vga.h>

static unsigned short* vga = (unsigned short*)VGA_MEMORY;
static int cursor_x = 0;
static int cursor_y = 0;
static unsigned char colour = 0;

static unsigned short make_entry(char c, unsigned char col)
{
    return (unsigned short)c | ((unsigned short)col << 8);
}

void vga_set_colour(vga_colour fg, vga_colour bg)
{
    colour = (bg << 4) | fg;
}

void vga_clear()
{
    for(int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    {
        vga[i] = make_entry(' ', colour);
    }

    cursor_x = 0;
    cursor_y = 0;
}

static void scroll()
{
    if(cursor_y < VGA_HEIGHT)
    {
        return;
    }

    // Move every row up by one
    for(int y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for(int x = 0; x < VGA_WIDTH; x++)
        {
            vga[y * VGA_WIDTH + x] = vga[(y + 1) * VGA_WIDTH + x];
        }
    }

    // Clear the last row
    for(int x = 0; x < VGA_WIDTH; x++)
    {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', colour);
    }

    cursor_y = VGA_HEIGHT - 1;
}

void vga_putchar(char c)
{
    if(c == '\n')
    {
        cursor_x = 0;
        cursor_y++;
    }
    else if(c == '\r')
    {
        cursor_x = 0;
    }
    else if(c == '\b')
    {
        // Backspace - move cursor back one 
        if(cursor_x > 0)
        {
            // only if we're not at the line start
            cursor_x--;
            // overwrite charcter with a space
            vga[cursor_y * VGA_WIDTH + cursor_x] = make_entry(' ', colour);
        }
    }
    else
    {
        vga[cursor_y * VGA_WIDTH + cursor_x] = make_entry(c, colour);
        cursor_x++;

        if(cursor_x >= VGA_WIDTH)
        {
            cursor_x = 0;
            cursor_y++;
        }
    }
    
    scroll();
}

void vga_print(const char* str)
{
    while(*str)
    {
        vga_putchar(*str++);
    }
}

void vga_init()
{
    vga_set_colour(WHITE, BLACK);
    vga_clear();
}