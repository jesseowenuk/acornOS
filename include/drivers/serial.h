#ifndef SERIAL_H
#define SERIAL_H

#include <file_system/vfs.h>

#include <stdint.h>

// COM1 base port address - the first serial port on x86
// All other COM1 registers are at offsets from this base
#define COM1 0x3F8

// Initialise the serial port - sets baud rate, data bits, parity
void serial_init();

// Write a single character to the serial port
void serial_putchar(char c);

// Print without a newline to the serial port
void serial_print(const char* str);

// Write a string followed by a newline to the serial port
void serial_println(const char* str);

int dev_serial_read(file_t* file, void* buffer, uint32_t size);
int dev_serial_write(file_t* file, const void* buffer, uint32_t size);

#endif