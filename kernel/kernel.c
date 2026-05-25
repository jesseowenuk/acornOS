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

// Create a test processes to verify everything works
void process_a()
{
    // This process just counts - we'll see it working via serial
    uint32_t count = 0;
    while(1)
    {
        count++;
        if(count % 10000000 == 0)
        {
            // Every ~10 million iterations
            serial_println("Process A running...");
        }
    }
}

static void process_b()
{
    uint32_t count = 0;
    while(1)
    {
        count++;
        if(count % 10000000 == 0)
        {
            serial_println("Process B running...");
        }
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

    // Create processes
    process_t* pa = process_create("process_a", process_a, 0);
    process_t* pb = process_create("process_b", process_b, 0);

    // Add to scheduler run queue
    scheduler_add(pa);
    scheduler_add(pb);

    serial_println("All subsystems online. Starting shell.");

    // Enable hardware interrupts.
    // From this point the CPU will respond to IRQs
    __asm__ volatile ("sti"); 

    // Start the shell
    shell_init();

    // Start the scheduling - never returns
    scheduler_start();

    // Hang forever - interrupts will fire keyboard_handler() for us
    for(;;); // hang
}

