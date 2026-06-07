#include "shadowfs.h"
#include "mem.h"            // For kmalloc, kfree
#include "pmm.h"            // For PAGE_SIZE
#include "kprintf.h"        // For kserial_printf
#include "string.h"         // For kmemset, kstrcpy etc.

// --- Forward declarations ---------------------------------------------
// Internal functions declared here, defined below
static inode_t* shadowfs_lookup(inode_t* dir, const char* name);
static inode_t* shadowfs_create(inode_t* dir, const char* name, uint32_t type);
static int      shadowfs_delete(inode_t* dir, const char* name);
static int      shadowfs_open(inode_t* inode, file_t* file);
static int      shadowfs_close(inode_t* inode, file_t* file);
static int      shadowfs_read(file_t* file, void* buf, uint32_t size);
static int      shadowfs_write(file_t* file, const void* buf, uint32_t size);
static int      shadowfs_readdir(file_t* file, dentry_t* dentry);
static int      shadowfs_mkdir(inode_t* dir, const char* name);
static int      shadowfs_truncate(inode_t* inode, uint32_t size);

// --- Operations table ------------------------------------------------
// Filled in with our function pointers
// Passed to vfs_mount() so VFS knows how to call us

static fs_ops_t shadowfs_ops =
{
    .lookup     = shadowfs_lookup,
    .create     = shadowfs_create,
    .delete     = shadowfs_delete,
    .truncate   = shadowfs_truncate,
    .open       = shadowfs_open,
    .close      = shadowfs_close,
    .read       = shadowfs_read,
    .write      = shadowfs_write,
    .seek       = 0,                // we use default VFS seek
    .readdir    = shadowfs_readdir,
    .mkdir      = shadowfs_mkdir,
    .rmdir      = 0,                // TODO: implement later
};

// --- shadowfs_get_ops --------------------------------------
// Returns the operations table for shadowFS
// VFS uses this to call out functions

fs_ops_t* shadowfs_get_ops()
{
    // Return pointer to our ops table
    return &shadowfs_ops;
}

static inode_t* shadowfs_lookup(inode_t* dir, const char* name)
{
    // TODO:
    (void)dir;
    (void)name;
    return 0;
}

static inode_t* shadowfs_create(inode_t* dir, const char* name, uint32_t type)
{
    // TODO:
    (void)dir;
    (void)name;
    (void)type;
    return 0;
}

static int      shadowfs_delete(inode_t* dir, const char* name)
{
    // TODO:
    (void)dir;
    (void)name;
    return 0;
}

static int      shadowfs_open(inode_t* inode, file_t* file)
{
    // TODO:
    (void)inode;
    (void)file;
    return 0;
}

static int      shadowfs_close(inode_t* inode, file_t* file)
{
    // TODO:
    (void)inode;
    (void)file;
    return 0;
}

static int      shadowfs_read(file_t* file, void* buf, uint32_t size)
{
    // TODO:
    (void)file;
    (void)buf;
    (void)size;
    return 0;
}

static int      shadowfs_write(file_t* file, const void* buf, uint32_t size)
{
    // TODO:
    (void)file;
    (void)buf;
    (void)size;
    return 0;
}

static int      shadowfs_readdir(file_t* file, dentry_t* dentry)
{
    // TODO:
    (void)file;
    (void)dentry;
    return 0;
}

static int      shadowfs_mkdir(inode_t* dir, const char* name)
{
    // TODO:
    (void)dir;
    (void)name;
    return 0;
}

static int      shadowfs_truncate(inode_t* inode, uint32_t size)
{
    // TODO:
    (void)inode;
    (void)size;
    return 0;
}