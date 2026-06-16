#include <architecture/x86_64/gdt.h>
#include <architecture/x86_64/idt.h>
#include <architecture/x86_64/paging.h>
#include <architecture/x86_64/pic.h>
#include <architecture/x86_64/tss.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <file_system/shadowfs.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/processes/process.h>
#include <kernel/processes/scheduler.h>
#include <kernel/processes/syscall.h>

#include "../apps/shell/shell.h"

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

// Test //////////////////////////////////////////////////

// The program that exec() will load
static const char exec_msg[] = "Hello from exec'd program!\n";

static void exec_program()
{
    __asm__ volatile(
        "mov $1, %%eax\n\t"             // SYS_WRITE
        "int $0x80\n\t"
        :
        : "b"(exec_msg), "c"(27)
        : "eax"
    );

    __asm__ volatile(
        "mov $0, %%eax\n\t"             // SYS_EXIT
        "mov $0, %%ebx\n\t"
        "int $0x80\n\t"
        :
        :
        : "eax", "ebx"
    );

    for(;;);
}

static const char wait_parent_msg[] = "Parent: forking...\n";
static const char wait_done_msg[] = "Parent: child done!\n";

static void wait_test_program()
{
    // Print parent message
    __asm__ volatile(
        "mov $1, %%eax\n\t"
        "int $0x80\n\t"
        :
        : "b"(wait_parent_msg), "c"(19)
        : "eax"
    );

    // Fork
    uint32_t pid;
    __asm__ volatile(
        "mov $5, %%eax\n\t"
        "int $0x80\n\t"
        "mov %%eax, %0\n\t"
        : "=r"(pid)
        :
        : "eax"
    );

    if(pid == 0)
    {
        // Child - exec a new program
        __asm__ volatile(
            "mov $7, %%eax\n\t"             // SYS_EXEC
            "int $0x80\n\t"
            :
            : "b"(exec_program)             // Address of new program
            : "eax"
        );

        // Should never reach here
        for(;;);
    }
    else
    {
        // Parent waits for child
        __asm__ volatile(
            "mov $6, %%eax\n\t"             // SYS_WAIT
            "int $0x80\n\t"
            :
            :
            : "eax"
        );

        // Child is done
        __asm__ volatile(
            "mov $1, %%eax\n\t"
            "int $0x80\n\t"
            :
            : "b"(wait_done_msg), "c"(20)
            : "eax"
        );

        // Parent exits
        __asm__ volatile(
            "mov $0, %%eax\n\t"
            "mov $0, %%ebx\n\t"
            "int $0x80\n\t"
            :
            :
            : "eax", "ebx"
        );
    }

    for(;;);
}

void kernel_main(uint64_t mem_map_addr, uint64_t mem_map_count, uint64_t highest_ram)
{       
    // Initialise serial first so we can log everything that follows
    serial_init();
    kserial_printf("Kernel started.\n");

    vga_init();                                 // Clear screen, set default colour

    kprintf("acornOS v0.1\n");
    kprintf("------------\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising GDT...\n");
    gdt_init();                                 // Set up memory segments   
    kserial_printf("GDT initialised.\n");    
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("GDT online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising TS...\n");
    tss_init();
    kserial_printf("TSS initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("TSS online.\n");
    
    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising IDT...\n");
    idt_init();                                 // Set up interrupt handlers
    kserial_printf("IDT initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("IDT online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising PIC...\n");
    pic_init();                                 // Remap hardware interrupts
    kserial_printf("PIC initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("PIC online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising timer...\n");
    timer_init();                               // Set up PIT at 100Hz
    kserial_printf("Timer initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Timer online\n");
                             
    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising keyboard...\n");
    keyboard_init();                            // Set up the keyboard
    kserial_printf("Keyboard initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Keyboard online\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising memory manager...\n");
    mem_init();                                 // Set up the heap
    kserial_printf("Memory manager initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Memory manager online\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising PMM...\n");
    pmm_init(mem_map_addr, mem_map_count, highest_ram);      // Pass E820 map from bootloader
    kserial_printf("PMM initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("PMM online\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising paging...\n");
    paging_init();
    kserial_printf("Paging initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Paging ready.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising process manager...\n");
    process_init();
    kserial_printf("Process manager initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Process manager online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising scheduler...\n");
    scheduler_init();
    kserial_printf("Scheduler initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Scheduler online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising syscalls...\n");
    syscall_init();
    kserial_printf("Syscalls initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("Syscalls online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising VFS...\n");
    vfs_init();
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("VFS online.\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Mounting shadowFS...\n");
    if(shadowfs_mount("/temp", 8 * 1024 * 1024) < 0)       // 8MB for temp files
    {
        kpanic("kernel: failed to mount shadowFS at /temp!");
    }
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf("shadowFS mounted.\n");

    mem_print_stats();
    int fd = vfs_open("/temp/test.txt", O_CREAT | O_WRONLY);
    kserial_printf("fd=%d\n", fd);

    if(fd >= 0)
    {
        const char* msg = "Hello from shadowFS!\n";
        kserial_printf("About to write...\n");
        int written = vfs_write(fd, msg, 21);
        kserial_printf("shadowFS: wrote %d bytes\n", written);
        vfs_close(fd);
    }
    else
    {
        kpanic("kernel: FAILED to create test file in shadowFS!");
    }

    kserial_printf("All subsystems online. Starting shell.\n");

    // Enable hardware interrupts.
    // From this point the CPU will respond to IRQs
    __asm__ volatile ("sti"); 

    // Create and add the idle process first
    // PID 0 is always the idle process by convention
    // Idle runs when nothing else wants to
    process_t* idle = process_create("idle", idle_process, 0);

    if(!idle)
    {
        kpanic("kernel: failed to create idle process!");
    }

    idle->time_slice = 1;       // Minimum time slice
    idle->ticks_remaining = 1;
    scheduler_add(idle);

    // Create and add the shell process
    process_t* shell = process_create("shell", shell_process, 0);

    if(!shell)
    {
        kpanic("kernel: failed to create shell process!");
    }
    scheduler_add(shell);

    scheduler_start();          // start scheduling - never returns

    // Hang forever - interrupts will fire keyboard_handler() for us
    for(;;); // hang
}

