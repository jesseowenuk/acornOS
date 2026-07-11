#include <drivers/serial.h>
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
// Write 'count' bytes from buffer to a file descriptor
// arg1 (RDI) = file descriptor (1=stdout, 2=stderr, or any fd from open())
// arg2 (RSI) = pointer to the buffer
// arg3 (RDX) = number of bytes to write
// returns number of bytes written in RAX, or -1 on error
static void sys_write(registers_t* regs)
{
    int fd = (int)regs->rdi;
    const void* buf = (const void*)regs->rsi;
    uint32_t count = (int32_t)regs->rdx;

    if(!buf)
    {
        regs->rax = -1;
        return;
    }

    // Guard aginst absurdly large writes
    if(count > 4096)
    {
        kserial_printf("sys_write: suspicious length %d\n", count);
        regs->rax = -1;
        return;
    }

    regs->rax = vfs_write(fd, buf, count);

}

// --- sys_read ----------------------------------------
// Read up to 'count' bytes from a file descriptor into a buffer
// arg1 (RDI) = file descriptor (0=stdin or any fd from open())
// arg2 (RSI) = pointer to the buffer
// arg3 (RDX) = number of bytes to read
// returns number of bytes read in RAX, or -1 on error
static void sys_read(registers_t* regs)
{
    int fd = (int)regs->rdi;
    void* buf = (void*)regs->rsi;
    uint32_t count = (uint32_t)regs->rdx;

    if(!buf)
    {
        regs->rax = -1;
        return;
    }

    if(count > 4096)
    {
        regs->rax = -1;
        return;
    }

    regs->rax = vfs_read(fd, buf, count);
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
    // RDI = pointer to a path string naming the ELF binary to run
    const char* path = (const char*)regs->rdi;

    kserial_printf("sys_exec: path=0x%lx\n", regs->rdi);

    if(!path)
    {
        kserial_printf("sys_exec: null path!\n");
        regs->rax = -1;
        return;
    }

    // Replace current process image with the ELF at 'path'
    // Never returns on success
    process_exec(path);

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

// --- sys_heap_grow -------------------------------------
// Grows the calling process's heap by 'increment' bytes, mapping
// whatever new pages are needed, and returns the address where the
// newly availble memory begins (the OLD heap end).
//
// arg1 (RDI) = increment in bytes. 0 just queries the current break
// without growing anything. Negative increments (shrinking) aren't
// supported yet - the heap only ever grows for now: free()'d memory is
// reused within the process via the libc's own free list instead of
// being handed back to the kernel.
static void sys_heap_grow(registers_t* regs)
{
    int64_t increment = (uint64_t)regs->rdi;

    if(!current_process)
    {
        regs->rax = (uint64_t)-1;
        return;
    }

    uint64_t old_end = current_process->heap_end;

    if(increment == 0)
    {
        // Just asking where the break currently is
        regs->rax = old_end;
        return;
    }

    if(increment < 0)
    {
        kserial_printf("sys_heap_grow: shrinking not supported\n");
        regs->rax = (uint64_t)-1;
        return;
    }

    uint64_t new_end = old_end + (uint64_t)increment;

    // Pages up to this address are already mapped (rounds old_end UP to
    // the next page boundary - see the walkthrough for why this is safe.)
    uint64_t mapped_up_to = (old_end + 0xFFF) & ~0xFFFUL;
    uint64_t new_mapped_up_to = (new_end + 0xFFF) & ~0xFFFUL;

    for(uint64_t va = mapped_up_to; va < new_mapped_up_to; va += PAGE_SIZE)
    {
        uint64_t physical = (uint64_t)pmm_alloc();

        map_page_in(current_process->page_dir, va, physical, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

        // Zero the new page via the direct map, so malloc() never hands
        // out memory with leftover physical-page contents in it.
        uint8_t* page = (uint8_t*)physical_to_virtual(physical);

        for(int i = 0; i < PAGE_SIZE; i++)
        {
            page[i] = 0;
        }
    }

    current_process->heap_end = new_end;

    regs->rax = old_end;
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
    sys_heap_grow,          // 14 - SYS_HEAP_GROW
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