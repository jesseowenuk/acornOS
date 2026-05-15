#include "pic.h"

// Write a byte to an I/O port
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Read a byte from an I/O port
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Small delay via port 0x80 (safe unused port)
static inline void  io_wait()
{
    outb(0x80, 0);
}

void pic_init()
{
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // Initialise both PICs in cascade mode
    outb(PIC1_COMMAND, 0x11);       // ICW1: init + ICW4 needed
    io_wait();
    outb(PIC2_COMMAND, 0x11);      
    io_wait();

    // ICW2: remap IRQ base address
    outb(PIC1_DATA, 0x20);          // Master: IRQs start at 32
    io_wait();
    outb(PIC2_DATA, 0x28);          // Slave: IRQs start at 40    
    io_wait();

    // ICW3: Tell PICs about each other
    outb(PIC1_DATA, 0x04);          // Master: slave on IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02);          // Slave: cascade identity    
    io_wait();

    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01);          
    io_wait();
    outb(PIC2_DATA, 0x01);          
    io_wait();

    // Restore masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(int irq)
{
    if(irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);        // Tell slave PIC
    }

    outb(PIC1_COMMAND, PIC_EOI);            // Always tell master PIC
}

void pic_mask(int irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if(irq >= 8)
    {
        irq -= 8;
    }
    outb(port, inb(port) | (1 << irq));
}

void pic_unmask(int irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if(irq >= 8)
    {
        irq -= 8;
    }
    outb(port, inb(port) & ~(1 << irq));
}