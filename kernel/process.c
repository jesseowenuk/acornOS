#include "process.h"
#include "mem.h"                // For kmalloc - allocating PCB structs
#include "pmm.h"                // For pmm_alloc - allocating process stacks
#include "paging.h"             // For page directory management
#include "vga.h"                // For process_print_all output
#include "kprintf.h"
#include "tss.h"
#include "scheduler.h"
#include "string.h"
#include "panic.h"

extern void iret_to_usermode();

// --- Global state ----------------------------------------

// Array of pointers to all processes
// Index = PID., NULL = empty slot
process_t* process_table[MAX_PROCESSES];

// Which process is currently running
// NULL = kernel is running, no process yet
process_t* current_process = 0;

// Next PID to hand out
// PID 0 = kernel idle process
// PID 1 = first real process
uint32_t next_pid = 0;

// --- Init -----------------------------------------------
// Creates a new process and adds it to the process table
// entry = function pointer - where the process starts executing
// flags = reserved for future use (user/kernel mode etc.)

void process_init()
{
    // Zero out the process table - all slots empty
    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        // NULL = empty slot
        process_table[i] = 0;
    }

    // Start PID counter at 0
    next_pid = 0;

    // No process running yet
    current_process = 0;

    kserial_printf("Process: subsystem initialised.\n");
}

process_t* process_create(const char* name, void(*entry)(), uint32_t flags)
{
    (void)flags;                    // Not used yet - will be for ring 3

    // Guard against null entry point
    if(!entry)
    {
        kpanic("process_create: null entry point!");
    }

    // Guard against null name
    if(!name)
    {
        kpanic("process_create: null process name!");
    }

    // Step 1: find a free slot in the process table
    int slot = -1;
    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] == 0)
        {
            // Empty slot found
            slot = i;
            break;
        }
    }

    if(slot == -1)
    {
        // No more room for processes
        kpanic("process_create: process table full!");
        return 0;
    }

    // Step 2: allocate memory for the PCB itself
    // kmalloc gives us a chunk of heap memory for the struct
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));

    // Step 3: zero out the PCB - clean slate
    kmemset(proc, 0, sizeof(process_t));
    proc->parent_pid = 0;                       // Kernel processes have no parent

    // Step 4: fill in the PCB fields
    proc->pid = next_pid++;                     // Assign next available PID
    kstrcpy(proc->name, name, 32);              // Copy process name
    proc->state = PROCESS_READY;                // Ready to run immediatley
    proc->time_slice = 10;                      // 10 timer ticks per time slice
                                                // at 100Hz that's 100ms
    proc->ticks_remaining = proc->time_slice;   // Start with full time slice
    proc->next = 0;                             // Not in scheduler queue yet

    // Step 5: Allocate a stack for this process
    // Each process needs its own stack - we get a fresh page from PMM
    proc->stack = (uint32_t)pmm_alloc();        // Allocate one 4KB page

    // Stack grows downward - top of stack is at the END of the page
    // We subtract 4 to leave room for the first push
    proc->stack_top = proc->stack + PROCESS_STACK_SIZE - 4;

    // Step 6: set up the initial CPU state
    // When this process first runs, the CPU will load these values
    // Zero all the registers first
    kmemset(&proc->cpu, 0, sizeof(cpu_state_t));

    proc->cpu.eip = (uint32_t)entry;            // Start executing at entry function
    proc->cpu.esp = proc->stack_top;            // Stack starts at top and grows down
    proc->cpu.ebp = proc->stack_top;            // Base pointer same as stack top
    proc->cpu.eflags = 0x200;                   // Enable interrupts (IF flag = bit 9)
                                                // 0x200 = 0000 0010 0000 0000
                                                // bit 9 set = interrupts enabled
    proc->cpu.cs = 0x08;                        // Kernel code segment selector
    proc->cpu.ds = 0x10;                        // Kernel data segment selector
    proc->cpu.ss = 0x10;                        // Kernel stack segment selector

    // Pre-load the entry point onto the process stack
    // When scheduler_start does 'ret' it pops this address
    // and jumps to the process_entry function
    uint32_t* stack = (uint32_t*)proc->stack_top;
    *stack = (uint32_t)entry;                   // Push entry point onto stack

    proc->cpu.esp = proc->stack_top;            // ESP points to the entry point
    proc->cpu.eip = (uint32_t)entry;            // EIP also set for direct jump
    proc->cpu.ebp = proc->stack_top;            // EBP same as ESP initially 

    // Step 7: Each process gets own directory with kernel mappings shared
    proc->page_dir = paging_clone_directory();

    if(!proc->page_dir)
    {
        kpanic("process_create: failed to clone page directory!");
        return 0;
    }

    // Step 8: add to process table
    process_table[slot] = proc;

    // Log creation
    kserial_printf("Process: created '%s' PID=%d entry=0x%x\n", name, proc->pid, (uint32_t)entry);

    return proc;
}

// --- process_exit ------------------------------------------------------

void process_exit(process_t* process)
{
    if(!process)
    {
        return;
    }

    // Mark as dead
    process->state = PROCESS_DEAD;

    kserial_printf("Process: '%s' exited.\n", process->name);

    // TODO: free stack and PCB memory
    // TODO: notify parent process
    // TODO: remove from scheduler queue
}

// --- process_print_all ----------------------------------------------
// Prints a table of all processes - our first 'ps' command!

void process_print_all()
{
    vga_set_colour(CYAN, BLACK);
    kprintf("\nPID STATE    NAME\n");
    kprintf("--- -------  ----\n");
    vga_set_colour(WHITE, BLACK);

    const char* state_names[] = 
    {
        "READY  ",                  // PROCESS_READY
        "RUNNING",                  // PROCESS_RUNNING
        "BLOCKED",                  // PROCESS_BLOCKED
        "DEAD   ",                  // PROCESS_DEAD
    };

    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] == 0)
        {
            // Skip empty slots
            continue;
        }

        process_t* proc = process_table[i];

        kprintf("%d    %s  %s\n", proc->pid, state_names[proc->state], proc->name);
    }
}

// --- process_block ----------------------------------------
// Marks a process as blocked so the scheduler skips it
// The process will not run again until process_wake is called

void process_block(process_t* proc)
{
    if(!proc)
    {
        // Safety check
        return;
    }

    // Mark as blocked
    // Scheduler will skip this process until it's woken up
    proc->state = PROCESS_BLOCKED;          
}

// --- process_wake ---------------------------------------
// Marks a blocked process as ready so the scheduler picks it up
// Called from interrupt handlers when the event a process was waiting for occurs

void process_wake(process_t* proc)
{
    if(!proc)
    {
        // safety check
        return;
    }

    if(proc->state != PROCESS_BLOCKED)
    {
        // Only wake blocked processes, ignore if already ready/running
        return;
    }

    // Mark as ready to run. Scheduler will pick it up on the next tick
    proc->state = PROCESS_READY;
}

// --- process_user_create_process -----------------------------------

process_t* create_user_process(const char* name, void (*entry)())
{
    // Guard against null arguments
    if(!name)
    {
        kpanic("create_user_process: null name!");
    }

    if(!entry)
    {
        kpanic("create_user_process: null entry point!");
    }

    // Step 1: find a free slot in the process table
    int slot = -1;

    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] == 0)
        {
            slot = i;
            break;
        }
    }

    if(slot == -1)
    {
        kpanic("create_user_process: process table full!");
        return 0;
    }

    // Step 2: Allocate PCB
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    kmemset(proc, 0, sizeof(process_t));
    proc->parent_pid = 0;                   // Set properly when forked

    // Step 3: fill in the basic details
    proc->pid  = next_pid++;
    kstrcpy(proc->name, name, 32);
    proc->state = PROCESS_READY;
    proc->time_slice = 10;
    proc->ticks_remaining = 10;
    proc->next = 0;

    // Step 4: allocate kernel stack
    // Used when this process triggers an interrupt
    // The CPU switches to this stack automatically via TSS
    proc->stack = (uint32_t)pmm_alloc();

    // Top of kernel stack
    proc->stack_top = proc->stack + PAGE_SIZE - 4;

    // Step 5: allocate user stack
    // This is what the process itself uses for function calls
    // Lives in user space - process can read and write it freely
    uint32_t user_stack = (uint32_t)pmm_alloc();

    // Step 6: clone kernel page directory
    proc->page_dir = paging_clone_directory();

    if(!proc->page_dir)
    {
        kpanic("create_user_process: failed to clone page directory!");
        return 0;
    }

    // Step 7: map user stack into process's virtual address space
    // Must switch to process's page directory to map into it
    // Just below 3GB - top of user space
    uint32_t user_stack_virt = 0xBFFFF000;

    // Map user stack - now goes into the current process's own page directory
    map_page_in(proc->page_dir,             // Map into THIS process's directory
                user_stack_virt,            // Virtual address
                user_stack,                 // Physical address
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
    );

    // Step 8: pre-load the kernel stack with an iret frame
    // When switch_context first runs this process it will
    // find this frame and iret into ring 3
    uint32_t* kstack = (uint32_t*)proc->stack_top;

    *kstack-- = 0x23;                               // SS - user stack segment (RPL=3)
    *kstack-- = user_stack_virt + PAGE_SIZE - 4;    // ESP - top of user stack
    *kstack-- = 0x200;                              // EFLAGS - interrupts enabled
    *kstack-- = 0x1B;                               // CS - user code segment (RPL=3)
    *kstack-- = (uint32_t)entry;                    // EIP - entry point
    *kstack-- = 0;                                  // EAX = 0 (initial value)

    // Step 9: Set up CPU state to point at our iret frame
    // When switch_context restores this process it will
    // restore these registers then ret to our iret stub
    kmemset(&proc->cpu, 0, sizeof(cpu_state_t));
    proc->cpu.esp = (uint32_t)kstack + 4;           // ESP points to the top of iret frame
    proc->cpu.eip = (uint32_t)iret_to_usermode;     // First thing we do is iret to ring 3
    proc->cpu.eflags = 0x200;                       // Interrupts enabled
    proc->cpu.cs = 0x08;                            // Kernel code for now
    proc->cpu.ds = 0x10;                            // Kernel data for now
    proc->cpu.ss = 0x10;                            // Kernel stack segment

    // Step 10: add to process table
    process_table[slot] = proc;

    kserial_printf("create_user_process: '%s' PID=%d entry=0x%x\n", name, proc->pid, (uint32_t)entry);

    return proc;
}

// --- process_fork -------------------------------------------------

pid_t process_fork()
{
    if(!current_process)
    {
        // No current process
        return -1;
    }

    // Step 1: find a free slot
    int slot = -1;
    for(int i = 0; i < MAX_PROCESSES; i++)
    {
        if(process_table[i] == 0)
        {
            slot = i;
            break;
        }
    }

    if(slot == -1)
    {
        kserial_printf("fork: process table full\n");
        return -1;
    }

    // Step 2: allocate child PCB
    process_t* child = (process_t*)kmalloc(sizeof(process_t));
    if(!child)
    {
        kserial_printf("fork: failed to allocate child PCB!\n");
        return -1;
    }

    // Step 3: copy parent PCB to child
    // This copies all fields including cpu state
    uint8_t* src = (uint8_t*)current_process;
    uint8_t* dst = (uint8_t*)child;
    for(uint32_t i = 0; i < sizeof(process_t); i++)
    {
        dst[i] = src[i];
    }

    // Step 4: assign new PID
    child->pid = next_pid++;

    // Child knows who its parent is 
    // Parent will use this to find
    // children that have exited
    child->parent_pid = current_process->pid;

    // Step 5: copy process name
    kstrcpy(child->name, current_process->name, 32);

    // Step 6: allocate new kernel stack
    child->stack = (uint32_t)pmm_alloc();
    if(!child->stack)
    {
        kserial_printf("fork: failed to allocate child stack!\n");
        kfree(child);
        return -1;
    }

    // Step 7: copy kernel stack contents
    uint8_t* parent_stack = (uint8_t*)current_process->stack;
    uint8_t* child_stack = (uint8_t*)child->stack;
    for(uint32_t i = 0; i < PAGE_SIZE; i++)
    {
        child_stack[i] = parent_stack[i];
    }

    // Step 8: update stack top
    child->stack_top = child->stack + PAGE_SIZE - 4;

    // Step 9: deep copy page directory
    // Child gets its own copy of all user space pages
    child->page_dir = paging_deep_copy_directory(current_process->page_dir);
    if(!child->page_dir)
    {
        kserial_printf("fork: failed to copy page directory!\n");
        pmm_free((void*)child->stack);
        kfree(child);
        return -1;
    }

    // Step 10: Set up child's kernel stack with a fresh iret frame
    // Child resumes at same point in user space as parent
    // but fork() returns 0 to the child
    uint32_t* kstack = (uint32_t*)child->stack_top;

    *kstack-- = 0x23;                       // SS - user stack segment
    *kstack-- = current_process->user_esp;  // ESP - user stack pointer
    *kstack-- = 0x200;                      // EFLAGS - interrupts enabled only
                                            // TF deliberatley cleared
    *kstack-- = 0x1B;                       // CS - user code segment
    *kstack-- = current_process->user_eip;  // EIP - return after int $0x80
    *kstack-- = 0;                          // EAX = 0 (fork returns 0 to child)

    // Step 11: set up child CPU state
    child->cpu.esp = (uint32_t)kstack + 4;  // Points to top of iret frame
    child->cpu.eip = (uint32_t)iret_to_usermode;    // child enters via iret
    child->cpu.eax = 0;
    child->cpu.eflags = 0x200;              // Clean EFLAGS
    child->cpu.cs = 0x08;                   // Kernel code for now
    child->cpu.ds = 0x10;                   // Kernel data
    child->cpu.ss = 0x10;                   // Kernel stack segment

    // Step 12: set child state to ready
    child->state = PROCESS_READY;
    child->ticks_remaining = child->time_slice;
    child->next = 0;

    // Step 13: add to process table and scheduler
    process_table[slot] = child;
    scheduler_add(child);

    kserial_printf("fork: created child PID=%d from parent PID=%d\n", child->pid, current_process->pid);

    // Return child PID to parent
    return child->pid;
}

// --- process_exec -----------------------------------------

int process_exec(void (*entry)())
{
    if(!entry)
    {
        kserial_printf("exec: null entry point!\n");
        return -1;
    }

    kserial_printf("exec: PID=%d replacing with entry=0x%x\n", current_process->pid, (uint32_t)entry);

    // Step 1: allocate user stack first
    uint32_t new_stack = (uint32_t)pmm_alloc();
    if(!new_stack)
    {
        kserial_printf("exec: failed to allocate user stack!\n");
        return -1;
    }

    // Step 2: Allocate a fresh kernel stack
    uint32_t new_kstack = (uint32_t)pmm_alloc();
    if(!new_kstack)
    {
        kserial_printf("exec: failed to allocate kernel_stack!\n");
        return -1;
    }
    uint32_t new_kstack_top = new_kstack + PAGE_SIZE - 4;

    // Step 3: allocate a new page directory
    // We need a fresh address space for the new program
    // Clone kernel mappings
    // User space starts empty
    page_directory_t* new_dir = paging_clone_directory();

    if(!new_dir)
    {
        kserial_printf("exec: failed to allocate page directory!\n");
        pmm_free((void*)new_stack);
        pmm_free((void*)new_kstack);
        return -1;
    }


    // Step 4: map user stack into new page directory
    uint32_t user_stack_virt = 0xBFFFF000;
    map_page_in(
        new_dir,
        user_stack_virt,
        new_stack,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
    );

    // Step 5: set up iret frame on new kernel stack
    uint32_t* kstack = (uint32_t*)new_kstack_top;
    *kstack-- = 0x23;                                   // SS - user stack segment
    *kstack-- = user_stack_virt + PAGE_SIZE - 4;        // ESP - top of the user stack
    *kstack-- = 0x200;                                  // EFLAGS - interrupts enabled
    *kstack-- = 0x1B;                                   // CS - user code segment
    *kstack-- = (uint32_t)entry;                        // EIP - new entry point
    *kstack-- = 0;                                      // EAX - return value

    // Step 6: update process fields
    current_process->stack = new_kstack;
    current_process->stack_top = new_kstack_top;
    current_process->page_dir = new_dir;

    // Step 7: Update TSS with new kernel stack
    tss_set_kernel_stack(new_kstack_top);

    // Step 8: update CPU state to use new stack and entry point
    uint32_t new_esp = (uint32_t)kstack + 4;
    current_process->cpu.esp = new_esp;
    current_process->cpu.eip = (uint32_t)iret_to_usermode;
    current_process->cpu.eflags = 0x200;
    current_process->cpu.cs = 0x08;
    current_process->cpu.ds = 0x10;
    current_process->cpu.ss = 0x10;
    current_process->cpu.eax = 0;

    kserial_printf("exec: switching to new program\n");

    // Step 9: Switch to new directory and jump
    paging_switch_directory(new_dir);

    // Switch to new kernel stack and jump
    __asm__ volatile(
        "mov %0, %%esp\n\t"             // Switch to new kernel stack (%0 = first input operand (2nd : below))
        "jmp iret_to_usermode\n\t"      // Jump to user mode entry
        :
        : "r"(new_esp)
        : "memory"
    );

    // Never reached
    return -1;
}