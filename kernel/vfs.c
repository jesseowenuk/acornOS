#include "vfs.h"
#include "kprintf.h"
#include "string.h"
#include "mem.h"

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

// --- vfs_open ---------------------------------------------

int vfs_open(const char* path, uint32_t flags)
{
    // Step 1: find the inode for this path
    inode_t* inode = vfs_resolve_path(path);

    // Step 2: if file doesn't exist and O_CREAT set - create it
    if(!inode)
    {
        if(!(flags & O_CREAT))
        {
            kserial_printf("VFS: file not found: %s\n", path);

            // File doesn't exist and no O_CREAT
            return -1;
        }

        // Find the parent directory
        // e.g. for "home/jesse/notes.txt" parent is "home/jesse"
        char parent_path[VFS_MAX_PATH];
        char filename[VFS_MAX_NAME];

        // Split path into parent and filename
        int last_slash = -1;
        int plen = kstrlen(path);

        for(int i = plen - 1; i >= 0; i++)
        {
            if(path[i] == '/')
            {
                last_slash = i;
                break;
            }
        }

        if(last_slash <= 0)
        {
            // File is in root directory
            // keep initial slash
            parent_path[0] = '/';
            parent_path[1] = 0;
            kstrcpy(filename, path + 1, VFS_MAX_NAME);
        }
        else
        {
            // Copy parent path
            for(int i = 0; i < last_slash; i++)
            {
                parent_path[i] = path[i];
            }

            // Null terminate parent path
            parent_path[last_slash] = 0;

            // Copy filename
            kstrcpy(filename, path + last_slash + 1, VFS_MAX_NAME);
        }

        // Find parent directory inode
        inode_t* parent = vfs_resolve_path(parent_path);

        if(!parent)
        {
            kserial_printf("VFS: parent directory not found: %s\n", parent_path);
            return -1;
        }

        // Create the file
        if(!parent->ops || parent->ops->create == 0)
        {
            kserial_printf("VFS: filesystem doesn't support create!\n");
            return -1;
        }

        inode = parent->ops->create(parent, filename, VFS_TYPE_FILE);

        if(!inode)
        {
            kserial_printf("VFS: failed to create file: %s\n", path);
            return -1;
        }
    }

    // Step 3: allocate a file struct
    file_t* file = (file_t*)kmalloc(sizeof(file_t));

    if(!file)
    {
        kserial_printf("VFS: failed to allocate file struct!\n");
        return -1;
    }

    // Step 4: fill in the file struct

    // Which file
    file->inode = inode;

    // Start at the beginning
    file->position = 0;

    // O_RDONLY, O_WRONLY etc.
    file->flags = flags;

    // One reference (this fd)
    file->ref_count = 1;

    // Filesystem fills this in via open()
    file->private_date = 0;

    // Step 5: if O_TRUNC set - truncate file to zero length
    if(flags & O_TRUNC)
    {
        if(inode->ops && inode->ops->truncate != 0)
        {
            // Set file size to 0
            inode->ops->truncate(inode, 0);
        }
    }

    // Step 6: call fileystem's open() if it has one
    // Some filesystems need to do setup when a file is opened
    if(inode->ops && inode->ops->open != 0)
    {
        int result = inode->ops->open(inode, file);

        if(result < 0)
        {
            kserial_printf("VFS: filesystem open() failed!\n");
            kfree(file);
            return -1;
        }
    }

    // Step 7: if O_APPEND set - seek to end of file
    if(flags & O_APPEND)
    {
        // Start writing at the end of the file
        file->position = inode->size;
    }

    // Step 8: allocate a file descriptor
    int fd = vfs_alloc_fd(file);

    if(fd < 0)
    {
        kserial_printf("VFS: no free file descriptors!\n");
        kfree(file);
        return -1;
    }

    kserial_printf("VFS: opened %s fd=%d\n", path, fd);

    // Return file descriptor to caller
    return fd;
}

// --- vfs_close ------------------------------------------------

int vfs_close(int fd)
{
    // Step 1: get the file struct for this fd
    file_t* file = vfs_get_file(fd);

    if(!file)   
    {
        kserial_printf("VFS: close() invalid fd=%d\n", fd);

        // Invalid fd
        return -1;
    }

    // Step 2: call filesystem's close() if it has one
    // Some filesystems need to flush data on close
    if(file->inode &&
       file->inode->ops &&
       file->inode->ops->close != 0)
    {
        file->inode->ops->close(file->inode, file);
    }

    // Step 3: decrement reference count
    file->ref_count--;

    // Step 4: if no more refences - free the file struct
    if(file->ref_count == 0)
    {
        // Free the file struct
        kfree(file);
    }

    // Step 5: free the file descriptor slot
    vfs_free_fd(fd);

    kserial_printf("VFS: closed fd=%d\n", fd);

    // Success
    return 0;
}

// --- vfs_read --------------------------------------------------

int vfs_read(int fd, void* buf, uint32_t size)
{
    // Step 1: get the file struct
    file_t* file = vfs_get_file(fd);

    if(!file)
    {
        kserial_printf("VFS: read() invalid fd=%d\n", fd);
        return -1;
    }

    // Step 2: Check file is open for reading
    if(file->flags == O_WRONLY)
    {
        kserial_printf("VFS: read() fd=%d not open for reading!\n", fd);
        return -1;
    }

    // Step 3: Check filesystem supports read
    if(!file->inode ||
       !file->inode->ops ||
       file->inode->ops->read == 0)
    {
        kserial_printf("VFS: filesystem doesn't support read!\n");
        return -1;
    }

    // Step 4: delegate to filesystem
    // Filesystem updates file->position after reading
    int bytes = file->inode->ops->read(file, buf, size);

    // Returns bytes read, -1 on error
    return bytes;
}