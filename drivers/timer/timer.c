#include <architecture/x86_64/idt.h>
#include <architecture/x86_64/pic.h>
#include <drivers/timer.h>
#include <drivers/vga.h>

// --- PIT ports --------------------------------------------
// The PIT has three channels - we use channel 0 which is wired to IRQ0
#define PIT_CHANNEL0    0x40        // Channel 0 data port - we write the frequency here
#define PIT_COMMAND     0x43        // Command port - we configure the PIT here

// --- I/O helpers --------------------------------------------

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"               // Write value to port
        :                           // No outout operands
        : "a"(value),               // value goes into AL register
          "Nd"(port)                // port goes into DX register
    );
}

// --- State ------------------------------------------------

static uint32_t ticks = 0;          // Total ticks since boot - incremented every IRQ0

// --- Clock display --------------------------------------------
// Writes directly to VGA memory to show uptime in top-right corner
// We do this without touching the cursor so it doesn't interfere with the shell

static void update_clock()
{
    uint32_t seconds = ticks / TIMER_FREQUENCY;     // Convert ticks to seconds
    uint32_t minutes = seconds / 60;                // Convert seconds to minutes
    uint32_t hours = minutes / 60;                  // Convert minutes to hours

    seconds %= 60;                                  // Remainder seconds after minutes
    minutes %= 60;                                  // Remainder minutes after hours

    // Format: "UP HH:MM:SS" - 11 characters wide
    // We write directly to VGA buffer at top-right corner (column 69, row 0)
    // Each VGA cell is 2 bytes: [ASCII][colour]
    // Colour 0x0A = light green on black
    char buf[11];

    // Build the string manually - no sprintf available yet!
    buf[0] = 'U';
    buf[1] = 'P';
    buf[2] = ' ';
    buf[3] = '0' + (hours / 10);                    // Tens digit of hours
    buf[4] = '0' + (hours % 10);                    // Units digit of hours
    buf[5] = ':';
    buf[6] = '0' + (minutes / 10);                  // Tens digit of minutes
    buf[7] = '0' + (minutes % 10);                  // Units digit of minutes
    buf[8] = ':';
    buf[9] = '0' + (seconds / 10);                  // Tens digit of seconds
    buf[10] = '0' + (seconds % 10);                 // Units digit of seconds

    // Write directly to VGA memory - top right corner
    unsigned short* vga = (unsigned short*)0xB8000;
    for(int i = 0; i < 11; i++)
    {
        // Row 0, column 69 onwards (80 - 11 = 69)
        vga[69 + i] = (unsigned short)buf[i] | (0x0A << 8);     // 0x0A = light green on black
    }
}

// --- IRQ0 handler --------------------------------------------
// Called by irq_handler() in idt.c every time PIT fires

void timer_handler(registers_t* regs)
{
    // We don't need the registers here
    (void)regs;
    
    // Increment tick counter
    ticks++;

    // Once per second (ever 180 ticks) redraw the clock
    if(ticks % TIMER_FREQUENCY == 0)
    {
        update_clock();
    }
}

// --- Public API --------------------------------------------

uint32_t timer_get_ticks()
{
    // Raw tick count since boot
    return ticks;
}

uint32_t timer_get_seconds()
{
    // Ticks divided by frequency = seconds
    return ticks / TIMER_FREQUENCY;
}

// --- Init --------------------------------------------

void timer_init()
{
    // The PIT runs at a base frequency of 1,193,180 Hz
    // We divide this by our desired frequency to get the reload value
    // reload = 1193180 / 100 = 11931
    uint32_t reload = 1193180 / TIMER_FREQUENCY;

    // Send command byte to PIT:
    // 0x36 = 0011011b
    //  bits 7-6: 00 = channel 0
    //  bits 5-4: 11 = send low byte then high byte
    //  bits 3-1: 011 = mode 3 (square wave generator)
    //  bit 0:  0 = binary counting
    outb(PIT_COMMAND, 0x36);

    // Send the reload value - low byte first, then high byte
    outb(PIT_CHANNEL0, reload & 0xFF);          // Low byte
    outb(PIT_CHANNEL0, (reload >> 8) & 0xFF);   // High byte
}