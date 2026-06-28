#ifndef NULL_DRIVER_H
#define NULL_DROVER_H

#include <file_system/vfs.h>

int dev_null_read(file_t* file, void* buffer, uint32_t size);
int dev_null_write(file_t* file, const void* buffer, uint32_t size);

#endif