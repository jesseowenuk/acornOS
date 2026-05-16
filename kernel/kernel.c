#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"

void kernel_main()
{       
    vga_init();                                 // Clear screen, set default colour
    vga_print("acornOS v0.1\n");
    vga_print("------------\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising GDT...\n");
    gdt_init();                                 // Set up memory segments       
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("GDT online.\n");
    
    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising IDT...\n");
    idt_init();                                 // Set up interrupt handlers
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("IDT online.\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising PIC...\n");
    pic_init();                                 // Remap hardware interrupts
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("PIC online.\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising  keyboard...\n");
    keyboard_init();                           // Register keyboard handler
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Keyboard online.\n");

    // Enable hardware interrupts.
    // From this point the CPU will respond to IRQs
    vga_set_colour(WHITE, BLACK);
    vga_print("Enabling interrupts.\n");
    __asm__ volatile ("sti"); 
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Interrupts enabled.\n");

    shell_init();

    // Hang forever - interrupts will fire keyboard_handler() for us
    for(;;); // hang
}