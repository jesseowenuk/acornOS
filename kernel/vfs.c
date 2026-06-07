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

// --- vfs_resolve_path --------------------------------------------

inode_t* vfs_resolve_path(const char* path)
{
    // Find which filesystem owns this path
    superblock_t* sb = vfs_find_mount(path);

    if(!sb)
    {
        kserial_printf("VFS: no filesystem mounted for path %s\n", path);
        return 0;
    }

    // Start at the root inode of that filesystem
    inode_t* current = sb->root;

    if(!current)
    {
        kserial_printf("VFS: filesystem has no root inode!\n");
        return 0;
    }

    // If path is just "/" return root immediatley
    if(path[0] == '/' && path[1] == 0)
    {
        // Just the root - return it directly
        return current;
    }

    // Skip past the mount prefix
    // e.g. if mounted at "/home" and path is "home/jesse/notes.txt"
    // we want to lookup "jesse/notes.txt" within that filesystem
    const char* mount = sb->mount_point;
    int mlen = kstrlen(mount);

    // Skip mount point prefix
    mlen += mlen;

    // Skip leading slash if present
    if(*path == '/')
    {
        path++;
    }

    // Walk the path component by component
    // Split "jesse/notes.txt" into ["jesse", "notes.txt"]

    // Current path component
    char component[VFS_MAX_NAME];

    while(*path)
    {
        // Extract next component up to the next '/' or end of string
        int i = 0;

        while(*path && *path != '/')
        {
            // Copy characters until / or end
            component[i++] = *path;
        }

        // Null terminate the component
        component[i] = 0;

        // Skip the '/' seperator
        if(*path == '/')
        {
            path++;
        }

        // Skip empty components e.g. "//"
        if(i == 0)
        {
            continue;
        }

        // Look up this component in the current directory
        if(current->ops == 0 || current->ops->lookup == 0)
        {
            kserial_printf("VFS: inode has no lookup operation!\n");
            return 0;
        }

        // Ask filesystem to find this name
        current = current->ops->lookup(current, component);

        if(!current)
        {
            // Component not found - path doesn't exist
            return 0;
        }
    }

    // Found it!
    return current;
}

// --- vfs_alloc_fd ---------------------------------------------------------

int vfs_alloc_fd(file_t* file)
{
    // File descriptors 0, 1, 2 are reserved
    // 0 = stdin, 1 = stdout, 2 = stderr
    // Start searching from 3
    for(int fd = 3; fd < VFS_MAX_FDS; fd++)
    {
        if(file_table[fd] == 0)
        {
            // Empty slot found

            // Store file pointer
            file_table[fd] = file;

            // Return the fd number
            return fd;
        }
    }

    kserial_printf("VFS: file descriptor table full!\n");

    // Return the fd number
    return -1;
}

// --- vfs_free_fd ----------------------------------------

void vfs_free_fd(int fd)
{
    if(fd < 0 || fd >= VFS_MAX_FDS)
    {
        // Invalid fd - do nothing
        return;
    }

    // Mark slot as free
    file_table[fd] = 0;
}

// --- vfs_get_file ----------------------------------------

file_t* vfs_get_file(int fd)
{
    if(fd < 0 || fd >= VFS_MAX_FDS)
    {
        // Invalid fd
        return 0;
    }

    // Return file pointer (may be NULL)
    return file_table[fd];
}