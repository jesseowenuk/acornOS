#include "vga.h"

void kernel_main()
{
    vga_init();
    vga_print("acornOS v0.1\n");
    vga_print("------------\n");
    vga_set_colour(WHITE, BLACK);
    vga_print("Kernel loaded successfully.\n");
    vga_set_colour(YELLOW, BLACK);
    vga_print("VGA driver online.\n"); 

    for(;;); // hang
}