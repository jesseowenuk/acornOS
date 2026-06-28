#ifndef RANDOM_H
#define RANDOM_H

#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>

#include <stdint.h>

int dev_random_read(file_t* file, void* buffer, uint32_t size);

#endif