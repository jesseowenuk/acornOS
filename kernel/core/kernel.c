#include <architecture/x86_64/gdt.h>
#include <architecture/x86_64/pic.h>
#include <architecture/x86_64/tss.h>
#include <drivers/keyboard.h>
#include <drivers/null.h>
#include <drivers/random.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <drivers/vga.h>
#include <file_system/devfs.h>
#include <file_system/shadowfs.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/interrupts.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>
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

static uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2)
{
    register uint64_t rax __asm__("rax") = num;
    register uint64_t rbx __asm__("rbx") = arg1;
    register uint64_t rcx __asm__("rcx") = arg2;

    __asm__ volatile(
        "int $0x80\n\t"
        : "+r"(rax)
        : "r"(rbx), "r"(rcx)
        :
    );

    return rax;
}

static uint64_t sys_fork_call()
{
    register uint64_t rax __asm__("rax") = 5;

    __asm__ volatile(
        "int $0x80\n\t"
        : "+r"(rax)
        :
        :
    );

    return rax;
}

// The program that exec() will load
static void child_process()
{
    kserial_printf("Child process running! PID=%d\n", current_process->pid);
    syscall(0, 0, 0);                   // SYS_EXIT
    for(;;);
}

static void fork_test()
{
    kserial_printf("fork test: starting, PID=%d\n", current_process->pid);

    uint64_t pid = sys_fork_call();
    kserial_printf("fork_test: fork returned pid=%lu PID=%d\n", pid, current_process->pid);

    if(pid == 0)
    {
        kserial_printf("fork_test: I am the child!\n");
    }
    else
    {
        kserial_printf("fork_test: I am the parent, child pid=%lu\n", pid);
    }

    syscall(0, 0, 0);           // SYS_EXIT
    for(;;);
}

static void exec_target()
{
    kserial_printf("exec_target: running! PID=%d\n", current_process->pid);
    syscall(0, 0, 0);           // SYS_EXIT
    for(;;);
}

static void exec_test()
{
    kserial_printf("exec_test: about to exec, PID=%d\n", current_process->pid);
    syscall(7, (uint64_t)exec_target, 0);       // SYS_EXEC
    kserial_printf("exec_test: should never reach here!\n");
    for(;;);
}

void kernel_main(uint64_t mem_map_addr, uint64_t mem_map_count, uint64_t highest_ram)
{       
    // Initialise serial first so we can log everything that follows
    serial_init();
    kserial_printf("Starting Kernel...\n");

    vga_init();                                 // Clear screen, set default colour

    kprintf("acornOS v0.1\n");
    kprintf("------------\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising GDT...");
    gdt_init();                                 // Set up memory segments   
    kserial_printf("GDT initialised.\n");    
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising TSS...");
    tss_init();
    kserial_printf("TSS initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");
    
    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising IDT...");
    idt_init();                                 // Set up interrupt handlers
    kserial_printf("IDT initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising PIC...");
    pic_init();                                 // Remap hardware interrupts
    kserial_printf("PIC initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising timer...");
    timer_init();                               // Set up PIT at 100Hz
    kserial_printf("Timer initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");
                             
    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising keyboard...");
    keyboard_init();                            // Set up the keyboard
    kserial_printf("Keyboard initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising memory manager...");
    mem_init();                                 // Set up the heap
    kserial_printf("Memory manager initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising PMM...");
    pmm_init(mem_map_addr, mem_map_count, highest_ram);      // Pass E820 map from bootloader
    kserial_printf("PMM initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising paging...");
    paging_init();
    kserial_printf("Paging initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising process manager...");
    process_init();
    kserial_printf("Process manager initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising scheduler...");
    scheduler_init();
    kserial_printf("Scheduler initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising syscalls...");
    syscall_init();
    kserial_printf("Syscalls initialised.\n");
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Initialising VFS...");
    vfs_init();
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    vga_set_colour(WHITE, BLACK);
    kprintf("Mounting shadowFS...");
    if(shadowfs_mount("/temp", 8 * 1024 * 1024) < 0)       // 8MB for temp files
    {
        kpanic("kernel: failed to mount shadowFS at /temp!");
    }
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");
    shadowfs_stats();

    vga_set_colour(WHITE, BLACK);
    kprintf("Mounting devFS...");
    if(devfs_mount("/devices") < 0)       
    {
        kpanic("kernel: failed to mount devFS at /devices!");
    }
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    // Register /devices/display - write prints to VGA
    vga_set_colour(WHITE, BLACK);
    kprintf("Registering /devices/display...");
    devfs_register("/devices", "display", 0, dev_display_write);
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    // Register /devices/keyboard
    vga_set_colour(WHITE, BLACK);
    kprintf("Registering /devices/keyboard...");
    devfs_register("/devices", "keyboard", dev_keyboard_read, 0);
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    // Register /devices/null
    vga_set_colour(WHITE, BLACK);
    kprintf("Registering /devices/null...");
    devfs_register("/devices", "null", dev_null_read, dev_null_write);
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    // Register /devices/serial
    vga_set_colour(WHITE, BLACK);
    kprintf("Registering /devices/random...");
    devfs_register("/devices", "random", dev_random_read, 0);
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    // Register /devices/random
    vga_set_colour(WHITE, BLACK);
    kprintf("Registering /devices/serial...");
    devfs_register("/devices", "serial", dev_serial_read, dev_serial_write);
    vga_set_colour(LIGHT_GREEN, BLACK);
    kprintf(" [DONE]\n");

    kserial_printf("All subsystems online. Starting shell.\n");

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

    // Read it back
    int fd_read = vfs_open("/temp/test.txt", O_RDONLY);
    kserial_printf("Read fd=%d\n", fd_read);

    if(fd_read >= 0)
    {
        char read_buffer[32];
        int bytes_read = vfs_read(fd_read, read_buffer, 21);
        kserial_printf("Read %d bytes: %s\n", bytes_read, read_buffer);
        vfs_close(fd_read);
    }

    // Test readdir
    int fd_dir = vfs_open("/temp", O_RDONLY);
    kserial_printf("Dir fd=%d\n", fd_dir);

    if(fd_dir >= 0)
    {
        dentry_t dentry;
        int result;
        while((result = vfs_readdir(fd_dir, &dentry)) > 0)
        {
            kserial_printf("readdir: %s type=%d\n", dentry.name, dentry.type);
        }
        vfs_close(fd_dir);
    }

    // Test mkdir
    int result = vfs_mkdir("/temp/testdir");
    kserial_printf("mkdir result=%d\n", result);

    // Verify it appears in readdir
    int fd_dir2 = vfs_open("/temp", O_RDONLY);
    if(fd_dir2 >= 0)
    {
        dentry_t dentry;
        while(vfs_readdir(fd_dir2, &dentry) > 0)
        {
            kserial_printf("readdir: %s type=%d\n", dentry.name, dentry.type);
        }
        vfs_close(fd_dir2);
    }

    // Test delete
    int del_result = vfs_delete("/temp/test.txt");
    kserial_printf("delete result=%d\n", del_result);

    // Verify it's gone
    int fd_dir3 = vfs_open("/temp", O_RDONLY);
    if(fd_dir3 >= 0)
    {
        dentry_t dentry;
        while(vfs_readdir(fd_dir3, &dentry) > 0)
        {
            kserial_printf("readdir: %s type=%d\n", dentry.name, dentry.type);
        }
        vfs_close(fd_dir3);
    }

    // Test Truncate via O_TRUNC flag
    int fd_trunc = vfs_open("/temp/trunc.txt", O_CREAT | O_WRONLY);
    if(fd_trunc >= 0)
    {
        vfs_write(fd_trunc, "Hello from shadowFS!", 21);
        vfs_close(fd_trunc);
    }

    // Now open with O_TRUNC to truncate to Zero
    fd_trunc = vfs_open("/temp/trunc.txt", O_TRUNC | O_WRONLY);
    kserial_printf("truncate fd=%d\n", fd_trunc);
    if(fd_trunc >= 0)
    {
        vfs_close(fd_trunc);
    }

    // Verify file is empty
    int fd_verify = vfs_open("/temp/trunc.txt", O_RDONLY);
    if(fd_verify >= 0)
    {
        char buffer[32];
        int bytes = vfs_read(fd_verify, buffer, 32);
        kserial_printf("after truncate: read %d bytes\n", bytes);
        vfs_close(fd_verify);
    }

    // test /devices/display
    int display_fd = vfs_open("/devices/display", O_WRONLY);
    if(display_fd >= 0)
    {
        vfs_write(display_fd, "Hello from devFS!\n", 18);
        vfs_close(display_fd);
    }

    // Test /devices/null
    int null_fd = vfs_open("/devices/null", O_WRONLY);
    if(null_fd >= 0)
    {
        vfs_write(null_fd, "This should be discarded\n", 25);
        vfs_close(null_fd);
        kserial_printf("null device: write discarded OK\n");
    }

    // Test /devices/serial
    int serial_fd = vfs_open("/devices/serial", O_WRONLY);
    if(serial_fd >= 0)
    {
        vfs_write(serial_fd, "Hello from /devices/serial!\n", 28);
        vfs_close(serial_fd);
    }

    // Test ls /devices
    int dev_fd = vfs_open("/devices", O_RDONLY);
    if(dev_fd >= 0)
    {
        dentry_t dentry;
        kserial_printf("devices:\n");
        while(vfs_readdir(dev_fd, &dentry) > 0)
        {
            kserial_printf("  %s\n", dentry.name);
        }
        vfs_close(dev_fd);
    }

    // Test /devices/random
    int random_fd = vfs_open("/devices/random", O_RDONLY);
    if(random_fd >= 0)
    {
        uint8_t buffer[8];
        vfs_read(random_fd, buffer, 8);
        kserial_printf("random bytes: %x %x %x %x %x %x %x %x\n",
            buffer[0], buffer[1], buffer[2], buffer[3],
            buffer[4], buffer[5], buffer[6], buffer[7]);
        vfs_close(random_fd);
    }

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

    // Test fork independently
    process_t* fork_proc = process_create("fork_test", fork_test, 0);
    if(fork_proc)
    {
        scheduler_add(fork_proc);
    }

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

