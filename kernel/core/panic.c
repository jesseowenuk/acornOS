#include <drivers/serial.h>
#include <drivers/vga.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>

void kpanic(const char* msg)
{
    // Disable interrupts immediatley
    __asm__ volatile("cli");

    // Print to serial first - VGA might be broken
    kserial_printf("\n\nKERNEL PANIC: %s\n", msg);
    kserial_printf("System halted.\n");

    // Print to VGA
    vga_set_colour(WHITE, RED);
    kprintf("\n\n*** KERNEL PANIC ***\n");
    kprintf("%s\n, msg");
    kprintf("\nSystem halted.");

    // Halt forever
    for(;;)
    {
        __asm__ volatile("hlt");
    }
}