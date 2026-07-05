#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/interrupts.h>
#include <kernel/memory/mem.h>
#include <kernel/processes/process.h>
#include <kernel/processes/scheduler.h>
#include <kernel/processes/syscall.h>

// --- sys_exit ---------------------------------------------------
// Terminate the current process
// arg1 (ebx) = exit code
static void sys_exit(registers_t* regs)
{
    if(!current_process)
    {
        kpanic("sys_exit: no current process!");
    }

    int exit_code = (int)regs->rdi;         // Get exit code from RDI

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
    const char* str = (const char*)regs->rdi;

    // ECX = string length
    uint32_t len = regs->rsi;

    kserial_printf("sys_write: str=0x%x len=%d\n", (uint64_t)str, len);

    if(!str)
    {
        // NULL pointer check
        regs->rax = -1;

        // return error
        return;
    }

    // Guard against absurdly large writes
    if(len > 4096)
    {
        kserial_printf("sys_write: suspicous length %d\n", len);
        regs->rax = -1;
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
    regs->rax = written;
}

// --- sys_read ----------------------------------------
// Read a single character from the keyboard
// returns character in eax
static void sys_read(registers_t* regs)
{
    // Block until key available
    char c = keyboard_getchar();

    // Return character in EAX
    regs->rax = (uint32_t)c;
}

// --- sys_pid ------------------------------------------
// Get the current process ID
// Return PID in EAX
static void sys_getpid(registers_t* regs)
{
    if(current_process)
    {
        // Return PID in EAX
        regs->rax = current_process->pid;
    }
    else
    {
        // No current process
        regs->rax = 0;
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

    // Update current process CPU state from interrupt frame
    // so the child resumes at the correct point
    current_process->cpu.rip = regs->rip;
    current_process->cpu.rsp = regs->rsp;
    current_process->cpu.rflags = regs->rflags;
    current_process->cpu.rax = regs->rax;
    current_process->cpu.rbx = regs->rbx;
    current_process->cpu.rcx = regs->rcx;
    current_process->cpu.rdx = regs->rdx;
    current_process->cpu.rsi = regs->rsi;
    current_process->cpu.rdi = regs->rdi;
    current_process->cpu.rbp = regs->rbp;

    pid_t child_pid = process_fork();

    // Return child PID to parent
    // Child will have EAX=0 from the copied CPU state
    regs->rax = (uint32_t)child_pid;
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
            regs->rax = exit_code;

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
        regs->rax = -1;
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
            regs->rax = exit_code;
            return;
        }
    }

    // Something went wrong
    regs->rax = -1;
}

// --- sys_exec ------------------------------------------
static void sys_exec(registers_t* regs)
{
    // EBX = entry point address
    void (*entry)() = (void(*)())regs->rdi;

    kserial_printf("sys_exec: rbx=0x%lx entry=0x%lx\n", regs->rdi, (uint64_t)entry);

    if(!entry)
    {
        kserial_printf("sys_exec: null entry point!\n");
        regs->rax = -1;
        return;
    }

    // Replace current process
    // Never returns on success
    process_exec(entry);

    // Only reached on failure
    regs->rax = -1;
}

// --- sys_open ------------------------------------------
static void sys_open(registers_t* regs)
{
    const char* path = (const char*)regs->rdi;
    uint32_t flags = (uint32_t)regs->rsi;

    if(!path)
    {
        regs->rax = -1;
        return;
    }

    int file_descriptor = vfs_open(path, flags);
    regs->rax = (uint64_t)file_descriptor;
}

// --- sys_close -----------------------------------------
static void sys_close(registers_t* regs)
{
    int file_descriptor = (int)regs->rdi;
    regs->rax = vfs_close(file_descriptor);
}

// --- sys_seek ------------------------------------------
static void sys_seek(registers_t* regs)
{
    int file_descriptor = (int)regs->rdi;
    uint32_t offset = (uint32_t)regs->rsi;
    int whence = (int)regs->rdx;
    regs->rax = vfs_seek(file_descriptor, offset, whence);
}

// --- sys_mkdir -----------------------------------------
static void sys_mkdir(registers_t* regs)
{
    const char* path = (const char*)regs->rdi;
    if(!path)
    {
        regs->rax = -1;
        return;
    }
    regs->rax = vfs_mkdir(path);
}

// --- sys_readdir ---------------------------------------
static void sys_readdir(registers_t* regs)
{
    int file_desciptor = (int)regs->rdi;
    dentry_t* dentry = (dentry_t*)regs->rsi;
    if(!dentry)
    {
        regs->rax = -1;
        return;
    }

    regs->rax = vfs_readdir(file_desciptor, dentry);
}

// --- sys_delete ----------------------------------------
static void sys_delete(registers_t* regs)
{
    const char* path = (const char*)regs->rdi;
    if(!path)
    {
        regs->rax = -1;
        return;
    }

    regs->rax = vfs_delete(path);
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
    sys_open,               // 8 - SYS_OPEN
    sys_close,              // 9 - SYS_CLOSE
    sys_seek,               // 10 - SYS_SEEK
    sys_mkdir,              // 11 - SYS_MKDIR
    sys_readdir,            // 12 - SYS_READDIR
    sys_delete,             // 13 - SYS_DELETE
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
        current_process->user_esp = regs->rsp;          // User ESP
        current_process->user_eip = regs->rip;              // User return address
    }

    // Syscall number is in EAX
    uint32_t syscall_num = regs->rax;

    if(syscall_num >= SYSCALL_COUNT)
    {
        // Invalid syscall number
        kserial_printf("Syscall: unknown syscall %d\n", syscall_num);

        // return error
        regs->rax = -1;
        return;
    }

    // Dispatch to the appropriate handler
    // Call the handler
    // This may modify regs->rax as the return value
    syscall_table[syscall_num](regs);
}

// --- syscall_init ------------------------------------------

void syscall_init()
{
    // Nothing to do here yet - IDT entry is set up in idt_init()
    // This function exists for future initalisation
    kserial_printf("Syscall: handler ready.\n");
}