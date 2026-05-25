#include "util.h"
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
#include "process.h"
#include "scheduler.h"

// The shell runs as a kernel process
static void shell_process()
{
    // Print the first prompt
    shell_init();

    while(1)
    {

        // Block until key available
        char c = keyboard_getchar();

        // Handle it
        shell_handle_key(c);
    }
}

// The idle process - runs when nothing else wants to
// hlt pauses the CPU until the next interrupt fires
// This saves power and reduces heat vs spinning in a loop
static void idle_process()
{
    while(1)
    {
        // Sleep until next interrupt
        // Timer IRQ wakes us every 10ms
        // Keyboard IRQ wakes us on key press
        // Scheduler then picks shell if it's ready
        __asm__ volatile ("hlt");
    }
}

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

    // Debug
    void* test_alloc = kmalloc(16);
    serial_print("DEBUG: test kmalloc after init = 0x");
    uint32_t addr = (uint32_t)test_alloc;
    char hex[9];
    const char* digits = "0123456789ABCDEF";
    for(int i = 7; i >= 0; i--) {
        hex[i] = digits[addr & 0xF];
        addr >>= 4;
    }
    hex[8] = 0;
    serial_println(hex);
    kfree(test_alloc);

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

    uint32_t* raw = (uint32_t*)0x50000;
    serial_print("RAW[0] size=");
    print_num_serial(raw[0]);
    serial_print(" RAW[1] free=");
    print_num_serial(raw[1]);
    serial_print(" RAW[2] next=");
    print_num_serial(raw[2]);
    serial_println("");

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising process manager...\n");
    process_init();
    serial_println("Process manager initialised.");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Process manager online.\n");

    process_print_offsets();

    vga_set_colour(WHITE, BLACK);
    vga_print("Initialising scheduler...\n");
    scheduler_init();
    serial_println("Scheduler initialised.");
    vga_set_colour(LIGHT_GREEN, BLACK);
    vga_print("Scheduler online.\n");

    serial_println("All subsystems online. Starting shell.");

    // Enable hardware interrupts.
    // From this point the CPU will respond to IRQs
    __asm__ volatile ("sti"); 

    // Create and add the idle process first
    // PID 0 is always the idle process by convention
    // Idle runs when nothing else wants to
    process_t* idle = process_create("idle", idle_process, 0);
    idle->time_slice = 1;       // Minimum time slice
    idle->ticks_remaining = 1;
    scheduler_add(idle);

    // Create and add the shell process
    process_t* shell = process_create("shell", shell_process, 0);
    scheduler_add(shell);

    scheduler_start();          // start scheduling - never returns

    // Hang forever - interrupts will fire keyboard_handler() for us
    for(;;); // hang
}

