#include "vfs.h"
#include "kprintf.h"

// --- Mount table ------------------------------------------------------
// Fixed array of mounted filesystems
// We walk this to find which filesystem owns a given path
static superblock_t mount_table[VFS_MAX_MOUNTS];

// How many filesystems are mounted
static int mount_count = 0;

// --- File descriptor table ---------------------------------------------
// For now a global table - later each process gets its own
// file_table[fd] = open file, NULL = fd not in use
static file_t* file_table[VFS_MAX_FDS];

// --- vfs_init ---------------------------------------------------------

void vfs_init()
{
    // Zero out the mount table
    for(int i = 0; i < VFS_MAX_MOUNTS; i++)
    {
        // Empty mount point
        mount_table[i].mount_point[0] = 0;      
        
        // No root inside
        mount_table[i].root = 0;

        // No operations
        mount_table[i].ops = 0;

        // No private data
        mount_table[i].private_data = 0;
    }

    // Zero out the file descriptor table
    for(int i = 0; i < VFS_MAX_FDS; i++)
    {
        // All fds start as unused
        file_table[i] = 0;
    }

    // No filesystems mounted yet
    mount_count = 0;

    kserial_printf("VFS: initialised.\n");
}