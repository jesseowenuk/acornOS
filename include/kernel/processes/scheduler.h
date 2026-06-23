#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <kernel/interrupts.h>
#include <kernel/processes/process.h>

// Initialise the scheduler
void scheduler_init();

// Add a process to the run queue
void scheduler_add(process_t* proc);

// Called by the timer IRQ every tick
// Decrements the current process's time slice
// Switches to the next process when the slice expires
void scheduler_tick(registers_t* regs);

// Yield - current process voluntarily gives up its time slice
void scheduler_yield();

// Start the scheduler
void scheduler_start();

#endif