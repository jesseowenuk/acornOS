#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "paging.h"     // For page_directory_t

// Size of the user space stack - 16KB
#define USER_STACK_SIZE 16384

// --- Process states -------------------------------------------
typedef enum
{
    PROCESS_READY       = 0,            // Waiting to be scheduled - has everything it needs
    PROCESS_RUNNING     = 1,            // Currently executing on the CPU
    PROCESS_BLOCKED     = 2,            // Waiting for an event (I/O, timer, lock)
    PROCESS_DEAD        = 3             // Finished - resources not yet freed
} process_state_t;

// --- CPU state -------------------------------------------
// This is everything we need to save when switching away from a process
// and restore when switching back to it
// The order matters - it must match how we push registers in assembly
typedef struct
{
    uint32_t eax;                       // General purpose registers
    uint32_t ebx; 
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;                       // Base pointer - used for stack frames
    uint32_t esp;                       // Stack pointer - top of the stack
    uint32_t eip;                       // Instruction pointer - where to resume execution
    uint32_t eflags;                    // CPU flags - carry, zero, sign etc.
    uint32_t cs;                        // Code segment selector
    uint32_t ds;                        // Data segment selector
    uint32_t ss;                        // Stack segment selector
} cpu_state_t;

// --- Process ID ------------------------------------------
typedef uint32_t pid_t;                 // Process ID type - just a number

// --- Stack size ------------------------------------------
#define PROCESS_STACK_SIZE 4096         // Each process gets a 4KB kernel stack
                                        // enough for reasonable call depth

// --- Maximum processes -----------------------------------
#define MAX_PROCESSES 16                // Maximum concurrent processes for now
                                        // we'll increase this later

// --- Process Control Block -------------------------------
// Everything the kernel needs to know about a process
typedef struct process
{
    pid_t               pid;                // Unique process ID - assigned at creation
    char                name[32];           // Human readable name - useful for debugging
    process_state_t     state;              // Current state (ready, running, blocked, dead)
    cpu_state_t         cpu;                // Saved CPU state - restored on context switch
    uint32_t            stack;              // Physical address of this process's stack
    uint32_t            stack_top;          // Address of top of stack (stack grows down)
    page_directory_t*   page_dir;           // This process's virtual memory map
                                            // NULL = use kernel page directory
    int32_t             time_slice;         // How many timer ticks this process gets
                                            // before being preempted
    int32_t             ticks_remaining;    // Ticks left in current time slice
    struct process*     next;               // Used by scheduler
} process_t;

// --- Process table --------------------------------------
// Global array of all processes - index is the PID
extern process_t* process_table[MAX_PROCESSES];
extern process_t* current_process;          // Which process is currently running
extern uint32_t   next_pid;                 // Need PID to assign

// --- Functions ------------------------------------------
// Inititalise the process subsystem
void process_init();         

// Create a new process
// entry = function to run
// flags = future use (user/kernel mode etc.)
process_t* process_create(const char* name, void(*entry)(), uint32_t flags_);

// Mark a process as dead
void process_exit(process_t* process);

// Print all processes - like a simple 'ps' command
void process_print_all();

// Defined in switch.asm - performs the actual context switch
// Saves old process CPU state and restores new process CPU state
void switch_context(process_t* old, process_t* new);

// Block the current process - it will not be scheduled until woken
void process_block(process_t* proc);

// Wake a blocked process - it will be scheduled again
void process_wake(process_t* proc);

// Create a process that runs in ring 3 (user mode)
// entry = virtual address where process starts executing
process_t* create_user_process(const char* name, void (*entry)());

#endif