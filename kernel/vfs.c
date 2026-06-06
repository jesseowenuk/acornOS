#include "vfs.h"
#include "kprintf.h"
#include "string.h"

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

// --- vfs_mount -----------------------------------------------------------

int vfs_mount(const char* path, fs_ops_t* ops, void* private_data)
{
    // Check we haven't hit the mount limit
    if(mount_count >= VFS_MAX_MOUNTS)
    {
        kserial_printf("VFS: mount table full!\n");
        return -1;
    }

    // Check for duplicate mount point
    for(int i = 0; i < mount_count; i++)
    {
        if(kstrcmp(mount_table[i].mount_point, path) == 0)
        {
            kserial_printf("VFS: %s is already mounted!\n", path);
            return -1;
        }
    }

    // Fill in the mount table entry
    superblock_t* sb = &mount_table[mount_count];

    // Copy mount point path
    kstrcpy(sb->mount_point, path, VFS_MAX_PATH);

    // Filesystem operations table
    sb->ops = ops;

    // Filesystem specific data
    sb->private_data = private_data;

    // Root inode set by filesystem init
    sb->root = 0;

    // Default flags
    sb->flags = 0;

    // Ask the filesystem for its root inode
    // Every filesystem must provide a lookup operation
    // We use it to get the root directory inode
    if(ops && ops->lookup)
    {
        // NULL parent = ask for root
        // Filesystem returns its root inode
        sb->root = ops->lookup(0, "/");
    }

    mount_count++;

    kserial_printf("VFS: mounted %s\n", path);

    // Success
    return 0;
}

// --- vfs_find_mount --------------------------------------------------

superblock_t* vfs_find_mount(const char* path)
{
    // Best matching mount point so far
    superblock_t* best = 0;

    // Length of best match so far
    int best_len = -1;

    for(int i = 0; i < mount_count; i++)
    {
        const char* mount = mount_table[i].mount_point;
        int mlen = kstrlen(mount);

        // Check if this mount point is a prefix of our path
        // e.g. "/home" is a prefix of "/home/jesse/notes.txt"
        int match = 1;

        for(int j = 0; j < mlen; j++)
        {
            if(mount[j] != path[j])
            {
                // Mismatch - not a prefix
                match = 0;

                break;
            }
        }

        if(match && mlen > best_len)
        {
            // This is a better match
            best = &mount_table[i];

            // Update best match length
            best_len = mlen;
        }
    }

    // Return longest matching mount point
    // NULL if no filesystem owns this path
    return best;
}