#ifndef TIMER_H
#define TIMER_H

#include <kernel/interrupts.h>

#include <stdint.h>

// How many times per second the timer fires
// 100Hz means one tick every 10ms - fine grained enough for uptime tracking
#define TIMER_FREQUENCY 100

void timer_init();              // Set up the PIT and register IRQ handler
void timer_handler(registers_t* regs);      // Called by irq_handler IRQ0
uint32_t timer_get_ticks();     // Return total ticks since boot
uint32_t timer_get_seconds();   // Return total seconds since boot

#endif