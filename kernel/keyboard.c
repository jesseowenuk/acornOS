#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "vga.h"
#include "shell.h"

// Port 0x60 is the keyboard data port
// Reading from it gives us the scancode of the last key event
#define KEYBOARD_DATA_PORT 0x60

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
            // Send to shell instead of direcltly to VGA
            shell_handle_key(c);
        }
    }
}

void keyboard_init()
{
    // Nothing to configure on the keyboard hardware for basic input
    // The PIC is already set up to receive IRQ1
    // We just need to register our handler - done in idt.c

    // Confirm initialisation
    vga_print("Keyboard online.\n");
}