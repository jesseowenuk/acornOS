#ifndef VGA_H
#define VGA_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xFFFF8000000B8000UL

// Colour constants
typedef enum
{
    BLACK = 0, 
    BLUE, 
    GREEN, 
    CYAN,
    RED, 
    MAGENTA, 
    BROWN, 
    LIGHT_GREY,
    DARK_GREY,
    LIGHT_BLUE,
    LIGHT_GREEN,
    LIGHT_CYAN,
    LIGHT_RED,
    LIGHT_MAGENTA,
    YELLOW,
    WHITE
} vga_colour;

void vga_init();
void vga_set_colour(vga_colour fg, vga_colour bg);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_clear();

#endif 