#ifndef PIC_H
#define PIC_H

#include <stdint.h>

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20        // End of interrupt signal

void pic_init();
void pic_send_eoi();
void pic_mask(int irq);
void pic_unmask(int irq);

#endif