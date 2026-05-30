#include "syscall.h"
#include "process.h"            // For current_process, process_exit
#include "scheduler.h"          // For scheduler_yield
#include "keyboard.h"           // For keyboard_getchar
#include "serial.h"             // For debug logging
#include "kprintf.h"

// --- sys_exit ---------------------------------------------------
// Terminate the current process
// arg1 (ebx) = exit code
static void sys_exit(registers_t* regs)
{
    int exit_code = (int)regs->ebx;         // Get exit code from EBX

    kserial_printf("Syscall: exit(%d)\n", exit_code);

    // Mark process as dead
    process_exit(current_process);   
    
    // Switch to the next process, this call never returns
    scheduler_yield();
}

// --- sys_write ---------------------------------------------------
// Write a string to the screen
// arg1 (ebx) = pointer to the string
// arg2 (ecx) = length of the string
// returns number of characters written in EAX
static void sys_write(registers_t* regs)
{
    // EBX = pointer to the string
    const char* str = (const char*)regs->ebx;

    // ECX = string length
    uint32_t len = regs->ecx;

    kserial_printf("sys_write: str=0x%x len=%d\n", (uint32_t)str, len);

    if(!str)
    {
        // NULL pointer check
        regs->eax -1;

        // return error
        return;
    }

    uint32_t written = 0;
    while(written < len && str[written])
    {
        // Write each character to VGA
        kprintf("%c", str[written]);
        written++;
    }

    // Return number of chars written
    regs->eax = written;
}

// --- sys_read ----------------------------------------
// Read a single character from the keyboard
// returns character in eax
static void sys_read(registers_t* regs)
{
    // Block until key available
    char c = keyboard_getchar();

    // Return character in EAX
    regs->eax = (uint32_t)c;
}

// --- sys_pid ------------------------------------------
// Get the current process ID
// Return PID in EAX
static void sys_getpid(registers_t* regs)
{
    if(current_process)
    {
        // Return PID in EAX
        regs->eax = current_process->pid;
    }
    else
    {
        // No current process
        regs->eax = 0;
    }
}

// --- sys_yield ----------------------------------------
// Voluntarily give up the CPU
static void sys_yield(registers_t* regs)
{
    // No arguments needed
    (void)regs;

    // Switch to next process
    scheduler_yield();
}

// --- Syscall dispatch table ----------------------------
// Array of function pointers - index = syscall number
// Makes adding new syscalls as simple as adding an entry here
typedef void (*syscall_fn)(registers_t*);

static syscall_fn syscall_table[] =
{
    sys_exit,               // 0 = SYS_EXIT
    sys_write,              // 1 = SYS_WRITE
    sys_read,               // 2 = SYS_READ
    sys_getpid,             // 3 = SYS_GETPID
    sys_yield,              // 4 = SYS_YIELD
};

// Number of syscalls in the table
// sizeof(array) / sizeof(element)
// gives us the element count
#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

// --- syscall_handler ------------------------------------
// Called from isr_handler when INT 0x80 fires
// Dispatches to the appropriate syscall function

void syscall_handler(registers_t* regs)
{
    // Syscall number is in EAX
    uint32_t syscall_num = regs->eax;

    if(syscall_num >= SYSCALL_COUNT)
    {
        // Invalid syscall number
        kserial_printf("Syscall: unknown syscall %d\n.", syscall_num);

        // return error
        regs->eax = -1;
        return;
    }

    // Log the syscall for debugging
    kserial_printf("Syscall: %d called.\n", syscall_num);

    // Dispatch to the appropriate handler
    // Call the handler
    // This may modify regs->eax as the return value
    syscall_table[syscall_num](regs);
}

// --- syscall_init ------------------------------------------

void syscall_init()
{
    // Nothing to do here yet - IDT entry is set up in idt_init()
    // This function exists for future initalisation
    kserial_printf("Syscall: handler ready.\n");
}