#include <architecture/x86_64/pic.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <kernel/core/kprintf.h>
#include <kernel/interrupts.h>
#include <kernel/processes/process.h>
#include <kernel/processes/scheduler.h>

#include "../apps/shell/shell.h"

// Port 0x60 is the keyboard data port
// Reading from it gives us the scancode of the last key event
#define KEYBOARD_DATA_PORT 0x60

// Process waiting for a keypress
process_t* keyboard_waiting = 0;
static char pending_key = 0;

// Read a byte from an I/O port
// We define it here again logically - later we'll move I/O helpers to a shared header
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile (
        "inb %1, %0"            // Read from port into value
        : "=a"(value)           // Output: store result in 'value' via EAX
        : "Nd"(port)            // Input: port number
    );
    return value;
}

// Scancode to ASCII lookup table
// Index = scancode, value = ASCII character (0 = no printable character)
// This covers the standard US keyboard layout, scancode 0x00-0x39
static const char scancode_table[] = 
{
    0,      0,      '1',    '2',    '3',    '4',    '5',    '6',    // 0x00-0x07
    '7',    '8',    '9',    '0',    '-',    '=',    '\b',   '\t',   // 0x08-0x0F
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',    // 0x10-0x17
    'o',    'p',    '[',    ']',    '\n',   0,      'a',    's',    // 0x18-0x1F
    'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',    // 0x20-0x27
    '\'',   '`',    0,      '\\',   'z',    'x',    'c',    'v',    // 0x28-0x2F
    'b',    'n',    'm',    ',',    '.',    '/',    0,      '*',    // 0x30-0x37    
    0,      ' ',    0                                               // 0x38-0x39     
};

// This is called by irq_handler() in idt.c every time IRQ1 fires
void keyboard_handler(registers_t* regs)
{
    // We don't need the registers here but the signatures must match
    (void)regs;

    // Read the scancode from the keyboard controller - this also
    // acknowledges the interrupt to the keyboard
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Bit 7 set means this is a key RELEASE. We only care about
    // key presses for now
    if(scancode & 0x80)
    {
        return;
    }

    // Look up the scancode in our table
    // Make sure the scancode is within our table
    if(scancode < sizeof(scancode_table))
    {
        // Get the ASCII character
        char c = scancode_table[scancode];

        // 0 means no printable character (e.g. shift, ctrl, function keys)
        if(c != 0)
        {
            // Store the key
            pending_key = c;

            // Wake the process that was waiting for input
            if(keyboard_waiting)
            {
                // Wake up the waiting process
                process_wake(keyboard_waiting);

                // clear the waiter
                keyboard_waiting = 0;       
            }
        }
    }
}

// Shell calls this to get the next key - blocks if none available
char keyboard_getchar()
{
    while(pending_key == 0)
    {
        // Block until key arrives
        keyboard_wait(current_process);
    }

    char c = pending_key;
    pending_key = 0;
    return c;
}

void keyboard_init()
{
    // Nothing to configure on the keyboard hardware for basic input
    // The PIC is already set up to receive IRQ1
    // We just need to register our handler - done in idt.c
}

void keyboard_wait(process_t* proc)
{
    // Register who is waiting
    keyboard_waiting = proc;

    // Block the process
    process_block(proc);

    // Tell PIC we're done with IRQ1
    pic_send_eoi(1);

    // Give up the CPU - don't return until woken
    scheduler_yield();
}

// --- devFS handler --------------------------------------------
// Read from /devices/keyboard - blocks until keypress
int dev_keyboard_read(file_t* file, void* buffer, uint32_t size)
{
    (void)file;

    char* dst = (char*)buffer;
    uint32_t read = 0;

    while(read < size)
    {
        // Blocks until key available
        dst[read++] = keyboard_getchar();
    }

    return (int)read;
}