#include <drivers/serial.h>

// --- I/O helpers ---------------------------

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value),               // value in AL
          "Nd"(port)                // port in DX
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)               // result into value via AL
        : "Nd"(port)                // port in DX
    );

    return value;
}

// -- UART register offsets from COM1 base (0x3F8)
// Each register is at a fixed offset from the base port address

#define DATA_REG        0           // 0x3F8 - read/write data
#define INT_ENABLE_REG  1           // 0x3F9 - interrupt enable
#define BAUD_LOW_REG    0           // 0x3F8 - baud rate low byte (when DLAB=1)
#define BAUD_HIGH_REG   1           // 0x3F9 - baud rate high byte (when DLAB=1)
#define FIFO_REG        2           // 0x3FA - FIFO control
#define LINE_CTRL_REG   3           // 0x3FB - line control (also sets DLAB)
#define MODEM_CTRL_REG  4           // 0x3FC - modem control
#define LINE_STATUS_REG 5           // 0x3FD - line status (is transmitter ready?)

// -- Init ----------------------------------------------------

void serial_init()
{
    // Disable all UART interrupts - we'll poll instead of using IRQs
    outb(COM1 + INT_ENABLE_REG, 0x00);

    // Enable  DLAB (Divisor Latch Access Bit) so we can set the baud rate
    // DLAB is bit 7 of the line control register
    // When DLAB=1, registers 0 and 1 become the baud rate divisor
    outb(COM1 + LINE_CTRL_REG, 0x80);

    // Set the baud rate to 38400
    // The UART base clock is 115200 Hz
    // Divisor = 115200 / 38400 = 3
    outb(COM1 + BAUD_LOW_REG, 0x03);            // Low byte of divisor
    outb(COM1 + BAUD_HIGH_REG, 0x00);           // High byte of divisor

    // Set the line control: 8 data bits, no parity, 1 stop bit (8N1)
    // This also clears DLAB (bit 7 = 0) so we're back to normal mode
    // 0x03 = 00000011b
    //  bits 0-1: 11 = 8 data bits
    //  bit 2:    0 = 1 stop bit
    //  bits 3-5: 000 = no parity
    //  bit 7:    0 = DLAB off
    outb(COM1 + LINE_CTRL_REG, 0x03);

    // Enable and clear FIFO buffers, set interrupt threshold to 14 bytes
    // 0xC7 = 11000111b
    //  bit 0: 1 = enable FIFO
    //  bit 1: 1 = clear receive FIFO
    //  bit 2: 1 = clear transmit FIFO
    //  bits 6-7: 11 = 14-byte interrupt threshold
    outb(COM1 + FIFO_REG, 0xC7);

    // Enable modem control - mark as ready to send and data terminal ready
    // 0x03 = 00000011b
    //  bit 0: 1 = DTR (Data Terminal Ready)
    //  bit 1: 1 = RTS (Request To Send)
    outb(COM1 + MODEM_CTRL_REG, 0x03);

    // Send a test message so we know serial is working
    serial_println("acornOS serial output online.");
    serial_println("-----------------------------");
}

// -- Transmit ----------------------------------------------------

// Check if the transmit buffer is empty and ready to accept a byte
// Bit 5 of the line status register = Transmitter Holding Register Empty
static int serial_is_ready()
{
    // Returns non-zero if ready, 0 if busy
    return inb(COM1 + LINE_STATUS_REG) & 0x20;
}

void serial_putchar(char c)
{
    // Spin until transmitter is ready
    // (polling so no interrupts needed)
    while(!serial_is_ready());

    // Write the character to the data register
    outb(COM1 + DATA_REG, c);
}

void serial_print(const char* str)
{
    // Walk the string until the null terminator
    while(*str)
    {
        // Before every newline send a carriage return - terminals expect \r\n
        if(*str == '\n')
        {
            serial_putchar('\r');
        }

        // Send the character and advance pointer
        serial_putchar(*str++);
    }
}

void serial_println(const char* str)
{
    // Print the string
    serial_print(str);

    // Then add a newline
    serial_print("\n");
}

// --- devFS handler --------------------------------------------
int dev_serial_read(file_t* file, void* buffer, uint32_t size)
{
    (void)file;
    // Serial read not yet implemented
    (void)buffer;
    (void)size;
    return 0;
}

int dev_serial_write(file_t* file, const void* buffer, uint32_t size)
{
    (void)file;
    const char* str = (const char*)buffer;
    for(uint32_t i = 0; i < size; i++)
    {
        serial_putchar(str[i]);
    }

    return (int)size;
}