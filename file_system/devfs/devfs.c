#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <file_system/devfs.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/core/string.h>
#include <kernel/memory/mem.h>

// --- Forward declarations ----------------------------------------
static inode_t* devfs_lookup(inode_t* dir, const char* name);
static inode_t* devfs_create(inode_t* dir, const char* name, uint32_t type);
static int devfs_open(inode_t* inode, file_t* file);
static int devfs_close(inode_t* inode, file_t* file);
static int devfs_read(file_t* file, void* buffer, uint32_t size);
static int devfs_write(file_t* file, const void* buffer, uint32_t size);
static int devfs_readdir(file_t* file, dentry_t* dentry);

// --- Operations table --------------------------------------------
static fs_ops_t devfs_ops =
{
    .lookup = devfs_lookup,
    .create = devfs_create,
    .open = devfs_open,
    .close = devfs_close,
    .read = devfs_read,
    .write = devfs_write,
    .readdir = devfs_readdir,
    .delete = 0,                    // Can't delete device files
    .mkdir = 0,                     // Can't create directories in devFS
    .rmdir = 0,
    .seek = 0,
    .truncate = 0,
};

fs_ops_t* devfs_get_ops()
{
    return &devfs_ops;
}

// --- Global mount instance --------------------------------------------
// Only one devFS mount supported for now
#define DEVFS_MAX_DEVICES 16
static devfs_mount_t devfs_mount_data;
static devfs_device_t device_table[DEVFS_MAX_DEVICES];

// --- Inode private data ------------------------------------------------
// Each device file inode points to its device entry
typedef struct
{
    devfs_device_t* device;                 // NULL = this is the root directory
} devfs_inode_t;

// --- Create a devFS inode -----------------------------------------------
static inode_t* devfs_create_inode(superblock_t* sb, uint32_t type, devfs_device_t* device)
{
    inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));
    kmemset(inode, 0, sizeof(inode_t));

    devfs_inode_t* private = (devfs_inode_t*)kmalloc(sizeof(devfs_inode_t));
    kmemset(private, 0, sizeof(devfs_inode_t));

    private->device = device;

    inode->type = type;
    inode->size = 0;
    inode->permissions = 0666;
    inode->link_count = 1;
    inode->ops = &devfs_ops;
    inode->sb = sb;
    inode->private_data = private;

    return inode;
}

// --- devfs_mount -------------------------------------------------
int devfs_mount(const char* path)
{
    // Initialise mount data
    kmemset(&devfs_mount_data, 0, sizeof(devfs_mount_t));
    kmemset(device_table, 0, sizeof(device_table));

    devfs_mount_data.devices = device_table;
    devfs_mount_data.count = 0;
    devfs_mount_data.capacity = DEVFS_MAX_DEVICES;

    // Register with VFS
    int result = vfs_mount(path, &devfs_ops, &devfs_mount_data);
    if(result < 0)
    {
        kserial_printf("devFS: vfs_mount failed!\n");
        return -1;
    }

    // Find superblock VFS created
    superblock_t* sb= vfs_find_mount(path);
    if(!sb)
    {
        kserial_printf("devFS: could not find superblock!\n");
        return -1;
    }

    // Create root directory inode
    inode_t* root = devfs_create_inode(sb, VFS_TYPE_DIR, 0);
    if(!root)
    {
        kserial_printf("devFS: failed to create root inode!\n");
        return -1;
    }

    sb->root = root;
    devfs_mount_data.root_inode = root;

    kserial_printf("devFS: mounted at %s\n", path);
    return 0;
}

// --- devfs_register -------------------------------------------
int devfs_register(const char* path, const char* name,
                    int (*read) (file_t*, void*, uint32_t),
                    int (*write) (file_t*, const void*, uint32_t))
{
    if(devfs_mount_data.count >= devfs_mount_data.capacity)
    {
        kserial_printf("devFS: device table full!\n");
        return -1;
    }

    // Find superblock
    superblock_t* sb = vfs_find_mount(path);
    if(!sb)
    {
        kserial_printf("devFS: mount not found at %s!\n", path);
        return -1;
    }

    // Add to device table
    devfs_device_t* dev = &device_table[devfs_mount_data.count];
    kstrcpy(dev->name, name, VFS_MAX_NAME);
    dev->read = read;
    dev->write = write;
    devfs_mount_data.count++;

    // Create inode for this device
    inode_t* inode = devfs_create_inode(sb, VFS_TYPE_FILE, dev);
    if(!inode)
    {
        kserial_printf("devFS: failed to create inode for %s!\n", name);
        return -1;
    }

    // Store inode pointer in device entry for lookup
    // We reuse the inode_num field as an index
    inode->inode_num = devfs_mount_data.count - 1;

    kserial_printf("devFS: registered device %s\n", name);
    return 0;
}

// --- devfs_lookup ----------------------------------------------
static inode_t* devfs_lookup(inode_t* dir, const char* name)
{
    if(!dir)
    {
        return 0;
    }

    superblock_t* sb = dir->sb;

    // Walk device table looking for matching name
    for(uint32_t i = 0; i < devfs_mount_data.count; i++)
    {
        if(kstreq(device_table[i].name, name))
        {
            // Found it - create a fresh inode for this device
            return devfs_create_inode(sb, VFS_TYPE_FILE, &device_table[i]);
        }
    }

    return 0;
}

// --- devfs_create ------------------------------------------------
static inode_t* devfs_create(inode_t* dir, const char* name, uint32_t type)
{
    // Device files are registered via devfs_register() not created by users
    (void)dir;
    (void)name;
    (void)type;
    kserial_printf("devFS: create not supported - use devfs_register()\n");
    return 0;
}

// --- devfs_open -------------------------------------------------
static int devfs_open(inode_t* inode, file_t* file)
{
    // Nothing to do - device is always ready
    (void)inode;
    (void)file;
    return 0;
}

// --- devfs_close -------------------------------------------------
static int devfs_close(inode_t* inode, file_t* file)
{
    // Nothing to do
    (void)inode;
    (void)file;
    return 0;
}

// --- devfs_read --------------------------------------------------
static int devfs_read(file_t* file, void* buffer, uint32_t size)
{
    if(!file || !file->inode)
    {
        return -1;
    }

    devfs_inode_t* private = (devfs_inode_t*)file->inode->private_data;
    if(!private || !private->device)
    {
        return -1;
    }

    if(!private->device->read)
    {
        kserial_printf("devFS: device %s does not support read\n", private->device->name);
        return -1;
    }

    return private->device->read(file, buffer, size);
}

// --- devfs_write --------------------------------------------
static int devfs_write(file_t* file, const void* buffer, uint32_t size)
{
    if(!file || !file->inode)
    {
        return -1;
    }

    devfs_inode_t* private = (devfs_inode_t*)file->inode->private_data;
    if(!private || !private->device)
    {
        return -1;
    }

    if(!private->device->write)
    {
        kserial_printf("devFS: device %s does not support write\n", private->device->name);
        return -1;
    }

    return private->device->write(file, buffer, size);
}

// --- devfs_readdir -----------------------------------------------
static int devfs_readdir(file_t* file, dentry_t* dentry)
{
    if(!file || !dentry)
    {
        return -1;
    }

    // file->position = index into device table
    uint32_t idx = file->position;

    if(idx >= devfs_mount_data.count)
    {
        // End of directory
        return 0;
    }

    kstrcpy(dentry->name, device_table[idx].name, VFS_MAX_NAME);
    dentry->type = VFS_TYPE_FILE;
    dentry->inode = 0;          // No inode pointer needed for listing

    file->position++;
    return 1;   
}