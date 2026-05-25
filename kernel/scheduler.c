#include "scheduler.h"
#include "process.h"        // For process_t, current_process
#include "timer.h"          // For timer_get_ticks
#include "serial.h"         // For debug logging
#include "vga.h"            // For vga_set_colour

// --- Forward declarations ---------------------------------
// switch_context is defined in switch.asm
// We declare it here so the compiler knows about it
extern void switch_context(process_t* old, process_t* new);

// --- Run queue ---------------------------------------------
// A simple circular linked list of processes waiting to run
// The scheduler walks this list picking the next process each tick.

static process_t* run_queue_head = 0;       // First process in the queue
static process_t* run_queue_tail = 0;       // Last process in the queue
                                            // tail->next = head (circular)

// --- scheduler_init ----------------------------------------

void scheduler_init()
{
    run_queue_head = 0;                     // Empty queue to start
    run_queue_tail = 0;
    serial_println("Scheduler: initialised.");
}

// --- scheduler_add ------------------------------------------
// Adds a process to the end of the run queue

void scheduler_add(process_t* proc)
{
    if(!proc)
    {
        return;
    }

    proc->state = PROCESS_READY;            // Mark as ready to run
    proc->next = 0;                         // Will be at the end of the queue

    if(run_queue_head == 0)
    {
        // Queue is empty - this process is both head and tail
        run_queue_head = proc;
        run_queue_tail = proc;
        proc->next = proc;                  // Point to itself - circular list
    }
    else
    {
        // Add to end of the queue
        run_queue_tail->next = proc;        // Old tail points to new process
        proc->next = run_queue_head;        // New process wraps back to head
        run_queue_tail = proc;              // Update tail pointer
    }

    serial_print("Scheduler: added process '");
    serial_print(proc->name);
    serial_println("' to run queue.");
}

// --- scheduler_tick --------------------------------------
// Called every timer tick - handles preemption
// This is called from irq_handler in idt.c

void scheduler_tick(registers_t* regs)
{
    (void)regs;                         // Not needed yet - will be for saving
                                        // register state directly from IRQ

    if(!current_process)
    {
        // No process running yet
        return;
    }

    if(!run_queue_head)
    {
        // Empty run queue
        return;
    }

    // Decrement the current process's remaining time slice
    current_process->ticks_remaining--;

    // If the process still has time left - let it continue
    if(current_process->ticks_remaining > 0 && current_process->pid != 0)
    {
        return;
    }

    // Either time slice has expired OR we're idle
    // In either case find the next runnable process
    process_t* old = current_process;       // Save pointer to outgoing process
    process_t* scan = old->next;             // Pick the next process in the queue
                                            // Since the list is circular this
                                            // always gives us a valid process
    process_t* best = 0;

    // Walk the queue looking for a non-dead, preferably non-idle process
    int checked = 0;
    while(checked < MAX_PROCESSES)
    {
        if(scan->state == PROCESS_READY)
        {
            // Found a runnable process
            best = scan;
            break;
        }
        
        // Skip this one
        scan = scan->next;
        checked++;
    }

    if(!best)
    {
        // Nothing ready - reset time slice and keep running current
        current_process->ticks_remaining = current_process->time_slice;
        return;
    }

    process_t* new = best;

    // Reset the new process's time slice
    new->ticks_remaining = new->time_slice;

    // Update states
    old->state = (old->state == PROCESS_READY) ? PROCESS_READY : old->state;             
    new->state = PROCESS_RUNNING;           // Incoming process is now running

    // Update current process pointer
    current_process = new;

    // Perform the actual context switch
    switch_context(old, new);               // Save old, restore new
                                            // This function may not return here
                                            // - execution resumes in new process
}

// --- scheduler_yield ---------------------------------
// Voluntary give up the current time slice
// Forces an immediate context switch to the next process.

void scheduler_yield()
{
    if(!current_process)
    {
        return;
    }

    current_process->ticks_remaining = 0;   // Expire the time slice
    scheduler_tick(0);                      // Force a switch immediatley
}

// --- scheduler_start --------------------------------
// Start the scheduler running - picks the first process and jumps to it
// This function never returns

void scheduler_start()
{
    if(!run_queue_head)
    {
        serial_println("Scheduler: no process to run!");
        return;
    }
    
    // Pick the first process
    current_process = run_queue_head;
    current_process->state = PROCESS_RUNNING;
    current_process->ticks_remaining = current_process->time_slice;

    serial_print("Scheduler: starting with process '");
    serial_print(current_process->name);
    serial_println("");

    // Set up the stack and jump to the process entry point
    // We do this in assemnbly to have full control
    __asm__ volatile (
        "mov %0, %%esp\n"           // Switch to the process's stack
        "sti\n"                     // Enable interrupts
        "ret\n"                     // Return to the process entry point
                                    // (EIP was pushed onto the process stack
                                    // during process_create)
        :
        : "r"(current_process->cpu.esp)
    );
}