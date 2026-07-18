#include <architecture/x86_64/tss.h>
#include <architecture/x86_64/usermode.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <kernel/core/elf.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/core/string.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>
#include <kernel/processes/process.h>
#include <kernel/processes/scheduler.h>

#include <stdint.h>

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
uint64_t next_pid = 0;

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

process_t* process_create(const char* name, void(*entry)(), uint64_t flags)
{
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

    // Boot-time processes (idle/hello/shell) have no parent - current_process
    // is NULL then. Anything created by a running process becomes it's child
    proc->parent_pid = current_process ? current_process->pid : 0;  

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
    uint64_t stack_physical = (uint64_t)pmm_alloc();
    kserial_printf("process_create: stack_physical=0x%x\n", (uint32_t)stack_physical);

    proc->stack = stack_physical + 0xFFFF800000000000UL;        // Convert to virtual via direct map

    // Stack grows downward - top of stack is at the END of the page
    // We subtract 4 to leave room for the first push
    proc->stack_top = proc->stack + PROCESS_STACK_SIZE - 8;
    kserial_printf("process_create: stack set up\n");

    // Step 6: set up the initial CPU state
    // When this process first runs, the CPU will load these values
    // Zero all the registers first
    kmemset(&proc->cpu, 0, sizeof(cpu_state_t));
    kserial_printf("process_create: cpu zeroed\n");

    if(flags & PROCESS_USER)
    {
        // Ring 3 process - elf_load will set up cpu state properly
        // Just set defaults here, elf_load overrides them
        proc->cpu.cs = 0x2B;
        proc->cpu.ds = 0x23;
        proc->cpu.ss = 0x23;
        proc->cpu.rflags = 0x200;
    }
    else
    {
        // Ring 0 kernel process
        proc->cpu.rip = (uint64_t)entry;            // Start executing at entry function
        proc->cpu.rsp = proc->stack_top;            // Stack starts at top and grows down
        proc->cpu.rbp = proc->stack_top;            // Base pointer same as stack top
        proc->cpu.rflags = 0x200;                   // Enable interrupts (IF flag = bit 9)
                                                    // 0x200 = 0000 0010 0000 0000
                                                    // bit 9 set = interrupts enabled
        proc->cpu.cs = 0x08;                        // Kernel code segment selector
        proc->cpu.ds = 0x10;                        // Kernel data segment selector
        proc->cpu.ss = 0x10;                        // Kernel stack segment selector

        // Pre-load the entry point onto the process stack
        // When scheduler_start does 'ret' it pops this address
        // and jumps to the process_entry function
        uint64_t* stack = (uint64_t*)(uintptr_t)proc->stack_top;
        *stack = (uint64_t)entry;                   // Push entry point onto stack
    }

    // Step 7: Each process gets own directory with kernel mappings shared
    proc->page_dir = paging_clone_directory();
    kserial_printf("process_create: page_dir set\n");

    if(!proc->page_dir)
    {
        kserial_printf("process_create: using kernel page tables for '%s'\n", name);
    }

    // Step 8: add to process table
    process_table[slot] = proc;

    // Log creation
    kserial_printf("Process: created '%s' PID=%d entry=0x%x\n", name, proc->pid, (uint64_t)entry);

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
        "SLEEP  ",                  // PROCESS_SLEEPING
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

// --- process_wait --------------------------------------------------
// Blocks the calling process until the specific child (by PID) exits,
// then frees its address space/kernel/stack and reaps its PCB.
// sys_exit() wakes us via process_wake() when any child of ours dies
// we just recheck whether it was the one we're actually waiting for.
void process_wait(pid_t pid)
{
    if(!current_process)
    {
        return;
    }

    while(1)
    {
        // Disable interrupts for the check-then-block sequence below   
        // otherwise the child could exit (via a timer interrupt
        // preempting us right here) and try to wake us in the gap
        // between "not dead yet" and actually marking ourselves
        // BLOCKED. process_wake() would see we're not BLOCKED yet and
        // silently  no-op, then we'd block anyway with nobody left to
        // ever wake us - a classic lost-wake up race. The "memory"
        // clobber matters - without it the compiler is free to reorder
        // or cache memory reads/writes across a bare asm("cli"), since
        // volatile alone only stops it deleting the instruction, not
        // reordering surrounding code around it:
        __asm__ volatile("cli" ::: "memory");

        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            process_t* child = process_table[i];

            if(child && child->pid == pid && child->state == PROCESS_DEAD)
            {
                __asm__ volatile("sti" ::: "memory");

                // Must unlink from the scheduler's own run queue before
                // freeing - otherwise the queue is left holding a
                // dangling pointer into memory we're about to kfree()
                scheduler_remove(child);
                paging_free_directory(child->page_dir);
                pmm_free((void*)virtual_to_physical(child->stack));
                process_table[i] = 0;
                kfree(child);
                return;
            }
        }

        // Not dead yet - block until sys_exit wakes us, then check
        // again. Marking ourselves BLOCKED must stay atomic with the
        // check above (still under cli); safe to re-enable
        // interrupts immediatley after, before yielding.
        process_block(current_process);
        __asm__ volatile("sti" ::: "memory");
        scheduler_yield();
    }
}

// --- process_spawn -------------------------------------------------
// Creates a brand new process and loads the ELF binary at 'path' into
// it, then adds it to the scheduler. Unlike exec(), this doesn't touch
// the calling process - used by the shell to run a program as a child
// while the shell keeps running.
// Return the new process's PID, or -1 on failure.
pid_t process_spawn(const char* path, char** argv)
{
    if(!path)
    {
        return -1;
    }

    void (*dummy)() = (void(*)())0x400000UL;
    process_t* proc = process_create(path, dummy, 0);
    if(!proc)
    {
        return -1;
    }

    if(!elf_load_from_path(path, proc, argv))
    {
        // Loading failed - tear down the half-built process
        paging_free_directory(proc->page_dir);
        pmm_free((void*)virtual_to_physical(proc->stack));

        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            if(process_table[i] == proc)
            {
                process_table[i] = 0;
                break;
            }
        }

        kfree(proc);
        return -1;
    }

    scheduler_add(proc);
    return proc->pid;
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
        kserial_printf("create_user_process: using kernel page tables\n");
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
    proc->stack = (uint64_t)pmm_alloc();

    // Top of kernel stack
    proc->stack_top = proc->stack + PAGE_SIZE - 8;

    // Step 5: allocate user stack
    // This is what the process itself uses for function calls
    // Lives in user space - process can read and write it freely
    uint64_t user_stack = (uint64_t)pmm_alloc();

    // Step 6: clone kernel page directory
    proc->page_dir = paging_clone_directory();

    if(!proc->page_dir)
    {
        kserial_printf("create_user_process: using kernel directory for now\n");
        return 0;
    }

    // Step 7: map user stack into process's virtual address space
    // Must switch to process's page directory to map into it
    // Just below 3GB - top of user space
    uint64_t user_stack_virt = 0xBFFFF000;

    // Map user stack - now goes into the current process's own page directory
    map_page_in(proc->page_dir,             // Map into THIS process's directory
                user_stack_virt,            // Virtual address
                user_stack,                 // Physical address
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
    );

    // Step 8: pre-load the kernel stack with an iret frame
    // When switch_context first runs this process it will
    // find this frame and iret into ring 3
    uint64_t* kstack = (uint64_t*)(uintptr_t)proc->stack_top;

    *kstack-- = 0x23;                               // SS - user stack segment (RPL=3)
    *kstack-- = user_stack_virt + PAGE_SIZE - 8;    // ESP - top of user stack
    *kstack-- = 0x200;                              // EFLAGS - interrupts enabled
    *kstack-- = 0x2B;                               // CS - user code segment (RPL=3)
    *kstack-- = (uint64_t)entry;                    // EIP - entry point
    *kstack-- = 0;                                  // EAX = 0 (initial value)

    // Step 9: Set up CPU state to point at our iret frame
    // When switch_context restores this process it will
    // restore these registers then ret to our iret stub
    kmemset(&proc->cpu, 0, sizeof(cpu_state_t));
    proc->cpu.rsp = (uint64_t)(kstack + 1);         // ESP points to the top of iret frame
    proc->cpu.rip = (uint64_t)iret_to_usermode;     // First thing we do is iret to ring 3
    proc->cpu.rflags = 0x200;                       // Interrupts enabled
    proc->cpu.cs = 0x08;                            // Kernel code for now
    proc->cpu.ds = 0x10;                            // Kernel data for now
    proc->cpu.ss = 0x10;                            // Kernel stack segment

    // Step 10: add to process table
    process_table[slot] = proc;

    kserial_printf("create_user_process: '%s' PID=%d entry=0x%x\n", name, proc->pid, (uint64_t)entry);

    return proc;
}

// --- process_fork -------------------------------------------------

pid_t process_fork()
{
    if(!current_process)
    {
        // No current process
        kpanic("fork: no current process!");
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
        kpanic("fork: process table full");
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
    for(uint64_t i = 0; i < sizeof(process_t); i++)
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
    uint64_t stack_physical = (uint64_t)pmm_alloc();
    if(!stack_physical)
    {
        kserial_printf("fork: failed to allocate child stack!\n");
        kfree(child);
        return -1;
    }

    child->stack = physical_to_virtual(stack_physical);
    if(!child->stack)
    {
        kserial_printf("fork: failed to allocate child stack!\n");
        kfree(child);
        return -1;
    }

    // Step 7: copy kernel stack contents
    uint8_t* parent_stack = (uint8_t*)(uintptr_t)current_process->stack;
    uint8_t* child_stack = (uint8_t*)(uintptr_t)child->stack;
    for(uint64_t i = 0; i < PAGE_SIZE; i++)
    {
        child_stack[i] = parent_stack[i];
    }

    // Step 8: update stack top
    child->stack_top = child->stack + PAGE_SIZE - 8;

    // Step 9: deep copy page directory
    // Child gets its own copy of all user space pages
    child->page_dir = paging_deep_copy_directory(current_process->page_dir);
    if(!child->page_dir)
    {
        kpanic("fork: failed to deep copy page directory!");
        return -1;
    }

    // Step 10: Set up child's kernel stack with a fresh iret frame
    // Child resumes at same point in user space as parent
    // but fork() returns 0 to the child
    if(!current_process->is_user)
    {
        // kernel mode process - simple resume
        // Just copy parent's CPU state, but return 0 from fork()
        child->cpu = current_process->cpu;
        child->cpu.rax = 0;                 // fork() returns 0 to child

        // Calculate RSP offset within parent's stack
        uint64_t rsp_offset = current_process->stack_top - current_process->cpu.rsp;
        
        // Apply same offset to child's stack
        child->cpu.rsp = child->stack_top - rsp_offset;  // Use child's own stack
        child->cpu.rbp = child->stack_top - (current_process->stack_top - current_process->cpu.rbp);
    }
    else
    {
        // User mode process - needs iret frame
        uint64_t* kstack = (uint64_t*)(uintptr_t)child->stack_top;

        *kstack-- = 0x23;                       // SS - user stack segment
        *kstack-- = current_process->user_esp;  // ESP - user stack pointer
        *kstack-- = 0x200;                      // EFLAGS - interrupts enabled only
                                                // TF deliberatley cleared
        *kstack-- = 0x2B;                       // CS - user code segment
        *kstack-- = current_process->user_eip;  // EIP - return after int $0x80
        *kstack-- = 0;                          // EAX = 0 (fork returns 0 to child)

        // Step 11: set up child CPU state
        child->cpu.rsp = (uint64_t)(kstack + 1);        // Points to top of iret frame
        child->cpu.rip = (uint64_t)iret_to_usermode;    // child enters via iret
        child->cpu.rax = 0;
        child->cpu.rflags = 0x200;              // Clean EFLAGS
        child->cpu.cs = 0x08;                   // Kernel code for now
        child->cpu.ds = 0x10;                   // Kernel data
        child->cpu.ss = 0x10;                   // Kernel stack segment
    }

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
// Replaces the current process's image with the ELF binary ayt 'path'
// Builds the new address space and kernel stack alongside the old ones
// (which keep running untouched) so a failed load leaves the calling
// process exactly as it was. Only on success do we commit: switch CR3,
// free the old image, and jump into the new one via enter_ring3() - the
// same ring3 entry mechanism a freshly created process uses.

int process_exec(const char* path, char** argv)
{
    if(!path)
    {
        kpanic("exec: null path!");
        return -1;
    }

    if(!current_process)
    {
        kpanic("exec: no current process!");
        return -1;
    }

    kserial_printf("exec: PID=%d replacing with '%s'\n", current_process->pid, path);

    // Remember the old image so we can free it on success, or fall back
    // to it if the new one fails to load
    page_directory_t* old_page_dir = current_process->page_dir;
    uint64_t old_stack = current_process->stack;
    uint64_t old_stack_top = current_process->stack_top;

    // Allocate a fresh kernel stack for the new image
    uint64_t new_kstack_physical = (uint64_t)pmm_alloc();
    uint64_t new_kstack = physical_to_virtual(new_kstack_physical);
    uint64_t new_kstack_top = new_kstack + PAGE_SIZE - 8;

    // Allocate a fresh address space - kernel mappings shared, user space empty
    page_directory_t* new_dir = paging_clone_directory();

    if(!new_dir)
    {
        kpanic("exec: failed to allocate page directory!");
        pmm_free((void*)new_kstack_physical);
        return -1;
    }

    // Point the process at the new resources so elf_load builds the
    // segments, user stack and iret frame in the new address space
    current_process->stack = new_kstack;
    current_process->stack_top = new_kstack_top;
    current_process->page_dir = new_dir;

    // Like 'path', argv and its strings point into the OLD image's user
    // space - safe to dereference here since elf_load() copies them
    // onto the NEW stack before this call returns, well before
    // old_page_dir gets freed below
    uint64_t entry = elf_load_from_path(path, current_process, argv);

    if(!entry)
    {
        kserial_printf("exec: failed to load '%s'\n", path);

        // Roll back - the old image is untouched, just discard the new one
        current_process->stack = old_stack;
        current_process->stack_top = old_stack_top;
        current_process->page_dir = old_page_dir;

        paging_free_directory(new_dir);
        pmm_free((void*)new_kstack_physical);

        return -1;
    }

    // Success - commit to the new image
    tss_set_kernel_stack(current_process->stack_top);
    paging_switch_directory(current_process->page_dir);

    // Free the old image now that we've switched away from it
    // Note: 'path' pointed into the OLD image's user space - it's no
    // longer valid to dereference once we've freed old_page_dir, so
    // don't reference it again below this point.
    paging_free_directory(old_page_dir);
    pmm_free((void*)virtual_to_physical(old_stack));

    kserial_printf("exec: PID=%d now running, entry=0x%lx\n", current_process->pid, entry);

    // Never returns - enters ring 3 using the frame elf_load() just built
    enter_ring3(current_process);

    // Never reached
    return -1;
}