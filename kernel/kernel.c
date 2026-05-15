#include "vga.h"
#include "gdt.h"
#include "idt.h"

void kernel_main()
{
    vga_init();
    vga_print("acornOS v0.1\n");
    vga_print("------------\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising GDT...\n");
    gdt_init();
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("GDT online.\n");
    
    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising IDT...\n");
    idt_init();
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("IDT online.\n");

    // Test: trigger a divide by zero exception
    vga_set_colour(WHITE, BLACK);
    vga_print("Kernel ready\n");

    for(;;); // hang
}