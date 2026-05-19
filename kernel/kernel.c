#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "shell.h"
#include "timer.h"
#include "mem.h"
#include "serial.h"
#include "pmm.h"
#include "paging.h"

void kernel_main(uint32_t mem_map_addr, uint32_t mem_map_count)
{       
    vga_init();                                 // Clear screen, set default colour

    // Initialise serial first so we can log everything that follows
    serial_init();
    serial_println("Kernel started.");

    vga_print("acornOS v0.1\n");
    vga_print("------------\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising GDT...\n");
    gdt_init();                                 // Set up memory segments   
    serial_println("GDT initialised.");    
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("GDT online.\n");
    
    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising IDT...\n");
    idt_init();                                 // Set up interrupt handlers
    serial_println("IDT initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("IDT online.\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising PIC...\n");
    pic_init();                                 // Remap hardware interrupts
    serial_println("PIC initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("PIC online.\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising timer...\n");
    timer_init();                               // Set up PIT at 100Hz
    serial_println("Timer initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Timer online\n");
                             
    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising keyboard...\n");
    keyboard_init();                            // Set up the keyboard
    serial_println("Keyboard initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Keyboard online\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising memory manager...\n");
    mem_init();                                 // Set up the heap
    serial_println("Memory manager initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Memory manager online\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising PMM...\n");
    pmm_init(mem_map_addr, mem_map_count);      // Pass E820 map from bootloader
    serial_println("PMM initialised.");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("PMM online\n");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising paging...\n");
    paging_init();
    serial_println("Paging initialised");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Paging ready.\n");

    serial_println("All subsystems online. Starting shell.");

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