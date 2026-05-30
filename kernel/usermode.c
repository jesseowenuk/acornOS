#include "usermode.h"
#include "tss.h"                // For tss_set_kernel_stack
#include "kprintf.h"            // For kserial_printf

// Defined in usermode.asm
extern void enter_usermode(uint32_t entry, uint32_t stack);

// The user stack - a page of memory for our user program
// We define it statically for now
// Later each process will get its own stack from PMM
static uint8_t user_stack[4096] __attribute__((aligned(4096)));

// jump_to_usermode - sets up kernel stack in TSS then enters ring 3
void jump_to_usermode(void (*entry)())
{
    // Update TSS so CPU knows where the kernel stack is
    // when this user program triggers an interrupt
    tss_set_kernel_stack(0x90000);
    kserial_printf("Usermode: jumping to ring 3 entry=0x%x\n", (uint32_t)entry);

    // Enter ring 3 - this never returns
    enter_usermode(
        (uint32_t)entry,                                // Entry point
        (uint32_t)user_stack + sizeof(user_stack) - 4   // Top of user stack - 4 for alignment
    );
}