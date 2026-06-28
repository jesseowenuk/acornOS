#include <file_system/vfs.h>

// --- /devices/null ---------------------------------------
// Write = discard silently
// Read = return EOF immediatley

int dev_null_read(file_t* file, void* buffer, uint32_t size)
{
    (void)file;
    (void)buffer;
    (void)size;
    return 0;           // EOF
}

int dev_null_write(file_t* file, const void* buffer, uint32_t size)
{
    (void)file;
    (void)buffer;
    return (int)size;   // Pretend we wrote everything
}