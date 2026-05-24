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

// Create a test process to verify everything works
void test_process()
{
    // This function represents a process
    // It won't actually run yet - we just create the PCB
    for(;;);
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

    process_create("kernel", 0, 0);         // PID 0 = kernel idle process
    process_create("test", test_process, 0);    // PID 1 = test process

    // Test map_page
    vga_set_colour(YELLOW, BLACK);
    vga_print("Testing map_page...\n");

    // Allocate a physical page from PMM
    void* phys = pmm_alloc();               // Get a free physical page

    // Map it to an arbitrary virtual address
    uint32_t virt = 0x400000;               // 4MB mark - just above our identity map
    map_page(virt, (uint32_t)phys, PAGE_PRESENT | PAGE_WRITABLE);

    // Write to the virtual address
    volatile uint32_t* ptr = (volatile uint32_t*)virt;
    *ptr = 0xDEADBEEF;                      // Write a test value

    // Read it back
    if(*ptr == 0xDEADBEEF)
    {
        vga_set_colour(LIGHT_GREEN, BLACK);
        vga_print("map_page test passed!\n");
    }
    else
    {
        vga_set_colour(RED, BLACK);
        vga_print("map_page test FAILED!\n");
    }

    // Unmap the page
    unmap_page(virt);
    vga_set_colour(WHITE, BLACK);

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

