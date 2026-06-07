#include "shadowfs.h"
#include "mem.h"            // For kmalloc, kfree
#include "pmm.h"            // For PAGE_SIZE
#include "kprintf.h"        // For kserial_printf
#include "string.h"         // For kmemset, kstrcpy etc.
#include "serial.h"

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

// --- Internal helpers --------------------------------------

// Create a new VFS inode for shadowFS
// type = VFS_TYPE_FILE or VFS_TYPE_DIR
// sb = superblock this inode belongs to
static inode_t* shadowfs_create_inode(superblock_t* sb, uint32_t type)
{
    // Get mount private data so we can check quota and assign inode numbers
    shadowfs_mount_t* mount = (shadowfs_mount_t*)sb->private_data;

    // Step 1: allocate the VFS inode struct
    inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));

    if(!inode)
    {
        kserial_printf("shadowFS: failed to allocate inode!\n");
        return 0;
    }

    // Step 2: zero it out
    kmemset(inode, 0, sizeof(inode_t));

    // Step 3: allocate private data
    // Files and directories have different private data layouts
    shadowfs_inode_t* priv = (shadowfs_inode_t*)kmalloc(sizeof(shadowfs_inode_t));

    if(!priv)
    {
        kserial_printf("shadowFS: failed to allocate inode private data!\n");
        kfree(inode);
        return 0;
    }

    kmemset(priv, 0, sizeof(shadowfs_inode_t));

    // Step 4: fill in the VFS inode fields

    // Assign a unique inode number
    inode->inode_num = mount->next_inode_num++;

    // File or directory
    inode->type = type;

    // Empty to start
    inode->size = 0;

    // rwxr-xr-x default
    inode->permissions = 0755;

    // One link (the directory entry)
    inode->link_count = 1;

    // Our operations table
    inode->ops = &shadowfs_ops;

    // Which filesystem we belong to
    inode->sb = sb;

    // Our private data
    inode->private_data = priv;

    // Step 5: - initialise private data based on type
    if(type == VFS_TYPE_FILE)
    {
        // No blocks yet - file is empty
        priv->file.blocks = 0;
        priv->file.block_count = 0;
    }
    else if(type == VFS_TYPE_DIR)
    {
        // No entries yet - empty directory
        priv->dir.entries = 0;
        priv->dir.count = 0;
    }

    // Return the inode
    return inode;
}

// --- shadowfs_mount ----------------------------------------------------
int shadowfs_mount(const char* path, uint32_t quota)
{
    // Step 1: check PMM has enough free memory for this quota
    uint32_t free_bytes = pmm_free_pages() * PAGE_SIZE;

    if(quota > free_bytes / 2)
    {
        // Refuse if quota > 50% of free RAM
        // Protects the rest of the system
        kserial_printf("shadowFS: quota %u exceeds 50% of free RAM!\n", quota);
        return -1;
    }

    // Step 2: allocate mount private data
    shadowfs_mount_t* mount = (shadowfs_mount_t*)kmalloc(sizeof(shadowfs_mount_t));

    if(!mount)
    {
        kserial_printf("shadowFS: failed to allocate mount data!\n");
        return -1;
    }

    kmemset(mount, 0, sizeof(shadowfs_mount_t));

    // Step 3: fill in the mount data
    
    // Maximum bytes this mount can use
    mount->quota = quota;

    // Nothing used yet
    mount->used = 0;

    // Start inode number at 1
    // 0 is reserved for "no inode"
    mount->next_inode_num = 1;

    // Set after mount
    mount->root_inode = 0;

    // Step 4: register with VFS
    int result = vfs_mount(path, &shadowfs_ops, mount);

    if(result < 0)
    {
        kserial_printf("shadowFS: vfs_mount failed!\n");
        kfree(mount);
        return -1;
    }

    // Step 5: find the superblock VFS just created
    superblock_t* sb = vfs_find_mount(path);
    
    if(!sb)
    {
        kserial_printf("shadowFS: could not find superblock!\n");
        kfree(mount);
        return -1;
    }

    // Step 6: create root directory inode
    inode_t* root = shadowfs_create_inode(sb, VFS_TYPE_DIR);

    if(!root)
    {
        kserial_printf("shadowFS: failed to create root inode!\n");
        kfree(mount);
        return -1;
    }

    // Step 7: wire root inode into superblock and mount
    sb->root = root;
    mount->root_inode = root;

    kserial_printf("shadowFS: mounted at %s quota=%uKB\n", path, quota / 1024);
    return 0;

}

// --- shadowfs_lookup ------------------------------------------------------
static inode_t* shadowfs_lookup(inode_t* dir, const char* name)
{
    // If dir is NULL - return root inode
    // This is called by vfs_resolve_path when looking up "/"
    if(!dir)
    {
        // Find our superblock and return root
        // We can't do this without a superblock reference
        // so return NULL for now - handled by vfs_mount directly
        return 0;
    }

    // Get private data for this directory
    shadowfs_inode_t* priv = (shadowfs_inode_t*)dir->private_data;

    if(!priv)
    {
        return 0;
    }

    // Walk the linked list of directory entries
    shadowfs_dentry_t* entry = priv->dir.entries;

    while(entry)
    {
        if(kstreq(entry->name, name))
        {
            // Found it
            return entry->inode;
        }

        // Next entry
        entry = entry->next;
    }

    // Not found
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