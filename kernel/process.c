#include "process.h"
#include "mem.h"                // For kmalloc - allocating PCB structs
#include "pmm.h"                // For pmm_alloc - allocating process stacks
#include "paging.h"             // For page directory management
#include "serial.h"             // For debug logging
#include "vga.h"                // For process_print_all output
#include "kprintf.h"

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

// --- String helpers --------------------------------------
// We don't have a standard library so we implement what we need.

static void kstrcpy(char* dst, const char* src, int max)
{
    int i = 0;
    while(src[i] && i < max - 1)
    {
        // Copy until null or max reached
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void kmemset(void* ptr, uint8_t value, uint32_t size)
{
    uint8_t* p = (uint8_t*)ptr;
    for(uint32_t i = 0; i < size; i++)
    {
        // Set each byte to value
        p[i] = value;
    }
}

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
        kserial_printf("process_create: process table full!\n");
        return 0;
    }

    // Step 2: allocate memory for the PCB itself
    // kmalloc gives us a chunk of heap memory for the struct
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));

    uint32_t eax_val;
    __asm__ volatile("mov %%eax, %0" : "=r"(eax_val));

    if(proc == 0)
    {
        kserial_printf("process_create: failed to allocate PCB!\n");
        return 0;
    }

    // Step 3: zero out the PCB - clean slate
    kmemset(proc, 0, sizeof(process_t));

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
    if(!proc->stack)
    {
        kserial_printf("process_create: failed to allocate stack!\n");
        kmalloc(0);             // TODO: free PCB
        return 0;
    }

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

    // Step 7: use the kernel page directory for now
    // Later each process will get its own - for now they share kernel mappings
    // 0 = use kernel page directory
    proc->page_dir = 0;

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