#ifndef DEVFS_H
#define DEVFS_H

#include <file_system/vfs.h>

#include <stdint.h>

// --- Device entry ----------------------------------------
// Each device file has custom read/write handlers
// that talk directly to the hardware driver
typedef struct
{
    // Device name e.g. "Keyboard"
    char name[VFS_MAX_NAME];

    // Read handler
    int (*read) (file_t* file, void* buffer, uint32_t size);
    int (*write) (file_t* file, const void* buffer, uint32_t size);
} devfs_device_t;

// --- Mount private data ---------------------------------------
// One per mounted devFS instance
typedef struct
{
    devfs_device_t* devices;        // Array of registered devices
    uint32_t count;                 // Number of devices registered
    uint32_t capacity;              // Size of devices array
    inode_t* root_inode;            // Root directory inode
} devfs_mount_t;

// --- Functions ------------------------------------------------

// Mount devFS at given path
int devfs_mount(const char* path);

// Register a device with devFS
// Must be called after devfs_mount()
int devfs_register(const char* path, const char* name,
                    int (*read) (file_t*, void*, uint32_t),
                    int (*write) (file_t*, const void*, uint32_t));

// Returns ops table for devFS
fs_ops_t* devfs_get_ops();

#endif