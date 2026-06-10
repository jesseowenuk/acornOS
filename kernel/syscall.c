#include "syscall.h"
#include "process.h"            // For current_process, process_exit
#include "scheduler.h"          // For scheduler_yield
#include "keyboard.h"           // For keyboard_getchar
#include "serial.h"             // For debug logging
#include "kprintf.h"
#include "pic.h"
#include "panic.h"

// --- sys_exit ---------------------------------------------------
// Terminate the current process
// arg1 (ebx) = exit code
static void sys_exit(registers_t* regs)
{
    if(!current_process)
    {
        kpanic("sys_exit: no current process!");
    }

    int exit_code = (int)regs->ebx;         // Get exit code from EBX

    kserial_printf("Syscall: exit(%d) PID=%d\n", exit_code, current_process->pid);

    // Save exit code for parent to read
    current_process->exit_code = exit_code;

    // Wake parent if it's waiting
    if(current_process->parent_pid > 0)
    {
        // Find current process
        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            if(process_table[i] && process_table[i]->pid == current_process->parent_pid && process_table[i]->state == PROCESS_BLOCKED)
            {
                // Parent is blocked - so lets wake it up!
                process_wake(process_table[i]);
                kserial_printf("exit: woke parent PID=%d\n", current_process->parent_pid);
                break;
            }
        }
    }

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
        regs->eax = -1;

        // return error
        return;
    }

    // Guard against absurdly large writes
    if(len > 4096)
    {
        kserial_printf("sys_write: suspicous length %d\n", len);
        regs->eax = -1;
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

// --- sys_fork ------------------------------------------
// Fork the current process
static void sys_fork(registers_t* regs)
{
    if(!current_process)
    {
        kpanic("sys_fork: no current process!");
    }

    pid_t child_pid = process_fork();

    // Return child PID to parent
    // Child will have EAX=0 from the copied CPU state
    regs->eax = (uint32_t)child_pid;
}

// --- sys_wait ------------------------------------------

static void sys_wait(registers_t* regs)
{
    (void)regs;

    kserial_printf("wait: PID=%d waiting for children\n", current_process->pid);

    // Look for any dead children first
    // If child already exited before we called wait - no need to block
    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] && process_table[i]->parent_pid == current_process->pid && process_table[i]->state == PROCESS_DEAD)
        {
            // Found a dead child - collect its exit code
            int exit_code = process_table[i]->exit_code;
            kserial_printf("wait: child PID=%d already dead exit=%d\n", process_table[i]->pid, exit_code);

            // Clean up the child

            // Remove from the process table
            process_table[i] = 0;

            // Return exit code to parent
            regs->eax = exit_code;

            return;
        }
    }

    // No dead children yet - check if we have any children at all
    int has_children = 0;
    for(int i = 0; i < MAX_PROCESSES;  i++)
    {
        if(process_table[i] && process_table[i]->parent_pid == current_process->pid)
        {
            has_children = 1;
            break;
        }
    }

    if(!has_children)
    {
        // No children - return error
        kserial_printf("wait: no children!\n");
        regs->eax = -1;
        return;
    }

    // Block until a child exits
    // sys_exit will wake us up when a child dies
    kserial_printf("wait: blocking until child exits\n");
    process_block(current_process);
    scheduler_yield();

    // When we wake up a child has exited - find it
    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] && process_table[i]->parent_pid == current_process->pid && process_table[i]->state == PROCESS_DEAD)
        {
            int exit_code = process_table[i]->exit_code;
            kserial_printf("wait: child PID=%d exited=%d\n", process_table[i]->pid, exit_code);
            regs->eax = exit_code;
            return;
        }
    }

    // Something went wrong
    regs->eax = -1;
}

// --- sys_exec ------------------------------------------
static void sys_exec(registers_t* regs)
{
    // EBX = entry point address
    void (*entry)() = (void(*)())regs->ebx;

    if(!entry)
    {
        kserial_printf("sys_exec: null entry point!\n");
        regs->eax = -1;
        return;
    }

    // Replace current process
    // Never returns on success
    process_exec(entry);

    // Only reached on failure
    regs->eax = -1;
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
    sys_fork,               // 5 - SYS_FORK
    sys_wait,               // 6 - SYS_WAIT
    sys_exec,               // 7 - SYS_EXEC
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
    if(!regs)
    {
        kpanic("syscall_handler: null registers!");
    }

    // Save user space state before handling syscall
    // Used by fork() to resume child at correct location
    if(current_process)
    {
        current_process->user_esp = regs->useresp;          // User ESP
        current_process->user_eip = regs->eip;              // User return address
    }

    // Syscall number is in EAX
    uint32_t syscall_num = regs->eax;

    if(syscall_num >= SYSCALL_COUNT)
    {
        // Invalid syscall number
        kserial_printf("Syscall: unknown syscall %d\n", syscall_num);

        // return error
        regs->eax = -1;
        return;
    }

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