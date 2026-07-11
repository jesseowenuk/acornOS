#ifndef VFS_H
#define VFS_H

#include <stdint.h>

// --- Constants --------------------------------------------------

#define VFS_MAX_MOUNTS          16                  // Maximum number of mounted file systems
#define VFS_MAX_FDS             256                 // Maximum open files per process
#define VFS_MAX_PATH            64
#define VFS_MAX_NAME            64                  // Maximum filename length

// --- File types -------------------------------------------------

#define VFS_TYPE_FILE           1                   // Regular file
#define VFS_TYPE_DIR            2                   // Directory
#define VFS_TYPE_DEVICE         3                   // Device file (keyboard, display etc.)
#define VFS_TYPE_SYMLINK        4                   // Symbolic link

// --- Open flags -------------------------------------------------

#define O_RDONLY                0x0                 // Open for reading only
#define O_WRONLY                0x1                 // Open for writing only
#define O_RDWR                  0x2                 // Open for reading and writing
#define O_CREAT                 0x4                 // Create file if it doesn't exist
#define O_APPEND                0x8                 // Append to file on every write
#define O_TRUNC                 0x10                // Truncate file to zero length on open

// --- Seek modes -------------------------------------------------

#define SEEK_SET                0                   // Seek from the start of the file
#define SEEK_CUR                1                   // Seek from current position
#define SEEK_END                2                   // Seek from the end of the file

// --- Forward declarations ---------------------------------------

struct inode;
struct file;
struct dentry;
struct superblock;

// --- Operations table -------------------------------------------
// Each filesystem fills in this table
// VFS calls through it - filesystem never called directly.

typedef struct fs_ops
{
    // --- Inode operations ----------------------------------------
    // Find a file/dir by name within a directory
    // Returns inode or NULL if not found
    struct inode* (*lookup)(struct inode* dir, const char* name);

    // Create a new file or directory
    // Returns new inode or NULL on failure
    struct inode* (*create)(struct inode* dir, const char* name, uint32_t type);

    // Deletes a file or directory
    // Returns 0 on success, -1 on failure
    int (*delete)(struct inode* dir, const char* name);

    // Resize a file to given size
    // Returns 0 on success, -1 on failure
    int (*truncate)(struct inode* inode, uint32_t size);

    // --- File operations ------------------------------------------
    // Open a file - called when process opens a file
    // file->flags contains O_RDONLY etc.
    // Returns 0 on success, -1 on failure
    int (*open)(struct inode* inode, struct file* file);

    // Close a file - called when process closes fd
    // Returns 0 on success, -1 on failure
    int (*close)(struct inode* inode, struct file* file);

    // Read size bytes from file at current position
    // Advances file->operation by bytes read
    // Returns bytes read, -1 on error
    int (*read)(struct file* file, void* buf, uint32_t size);

    // Write size bytes to file at current position
    // Advances file->position by bytes written
    // Returns bytes written, -1 on error
    int (*write)(struct file* file, const void* buf, uint32_t size);

    // Move file position
    // whence = SEEK_SET, SEEK_CUR or SEEK_END
    // Returns new position, -1 on error
    int (*seek)(struct file* file, int32_t offset, int whence);

    // --- Directory operations --------------------------------------------

    // Read next entry from directory
    // Fills in dentry with name and inode
    // Returns 1 if entry found, 0 if end, -1 on error
    int (*readdir)(struct file* file, struct dentry* dentry);

    // Create a new directory
    // Returns 0 on success, -1 on failure
    int (*mkdir)(struct inode* dir, const char* name);

    // Remove an empty directory
    // Returns 0 on success, -1 on failure
    int (*rmdir)(struct inode* dir, const char* name);
} fs_ops_t;

// --- Inode --------------------------------------------------
// Represents a file or directory
// One inode per file - multiple directory entries can point to same inode

typedef struct inode
{
    uint32_t            inode_num;                  // Unique ID within this filesystem
    uint32_t            type;                       // VFS_TYPE_FILE, VFS_TYPE_DIR etc.
    uint32_t            size;                       // File size in bytes
                                                    // For directories: number of entries
    uint32_t            permissions;                // rwxrwxrwx style permissions
    uint32_t            uid;                        // Owner user ID
    uint32_t            gid;                        // Owner group ID
    uint32_t            created;                    // Creation timestamp (seconds since boot)
    uint32_t            modified;                   // Last modification timestamp
    uint32_t            accessed;                   // Last access timestamp
    uint32_t            link_count;                 // Number of directory entries pointing here
    fs_ops_t*           ops;                        // Operations for this filesystem type
    struct superblock*  sb;                         // Which filesystem this inode belongs to
    void*               private_data;               // Filesystem specific data
                                                    // ramfs stores linked list of blocks here
                                                    // FAT32 stores cluster chain here
                                                    // acornFS stores hash content here
} inode_t;

// --- File ---------------------------------------------------------
// Represents an open file descriptor
// Created by open(), destroyed by close()

typedef struct file
{
    inode_t*        inode;                          // Which file does this descriptor refer to
    uint32_t        position;                       // Current read/write position in bytes
    uint32_t        flags;                          // O_RDOLY, O_WRONLY, O_RDWR etc.
    uint32_t        ref_count;                      // How many fds point to this file
                                                    // Incremented by dup(), decremented by close()
    void*           private_data;                   // Filesystem specific per-open-file data
} file_t;

// --- Directory entry ---------------------------------------
// Maps a filesystem to an inode
// Used by readdir() to list directory contents

typedef struct dentry
{
    char                name[VFS_MAX_NAME];         // Filename (not full path)
    inode_t*            inode;                      // Inode this name points to
    uint32_t            type;                       // VFS_TYPE_FILE or VFS_TYPE_DIR
                                                    // Cached here for efficiency        
} dentry_t;

// --- Superblock ------------------------------------------------------
// Represents a mounted filesystem
// One per mount point

typedef struct superblock
{
    // Where this filesystem is mounted
    // e.g. "/", "/dev", "mnt/usb"
    char mount_point[VFS_MAX_PATH];

    // Root inode of this filesystem
    inode_t* root;

    // Operations table for this filesystem
    fs_ops_t* ops;

    // Filesystem specific data
    // ramfs store root node list here
    // FAT32 stores PBP here
    void* private_data;

    // Mount flags (read-only etc.)
    uint32_t flags;
} superblock_t;

// --- VFS functions ------------------------------------------------------

// Initialise the VFS subsystem
void vfs_init();

// Mount a filesystem at the given path
// ops = filesystem operations table
// privated_data = filesystem specific data
// Returns 0 on success, -1 on failure
int vfs_mount(const char* path, fs_ops_t* ops, void* private_date);

// Unmount a filesystem
// Returns 0 on success, -1 on failure
int vfs_unmount(const char* path);

// --- File operations (called by syscalls) --------------------------------

// Open a file - returns file descriptor
// Creates file if O_CREAT set
// Returns fd >= 0 on success, -1 on failure
int vfs_open(const char* path, uint32_t flags);

// Like vfs_open, but installs the file at a specific fd instead of
// allocating one. Used to set up fd 0/1/2 (stdin/stdout/stderr) at boot.
int vfs_open_at(const char* path, uint32_t flags, int fd);

// Close a file descriptor
// Returns 0 on success, -1 on failure
int vfs_close(int fd);

// Read from open file
// Returns bytes read, -1 on error
int vfs_read(int fd, void* buf, uint32_t size);

// Write to open file
// Returns bytes written, -1 on error
int vfs_write(int fd, const void* buf, uint32_t size);

// Move file position
// Returns new position -1 on error
int vfs_seek(int fd, int32_t offset, int whence);

// Read directory entry
// Returns 1 if found, 0 if end, -1 on error
int vfs_readdir(int fd, dentry_t* dentry);

// Create a directory
// Returns 0 on sucess -1 on failure
int vfs_mkdir(const char* path);

// Delete a file
// Returns 0 on success, -1 on failure
int vfs_delete(const char* path);

// --- Path resolution -----------------------------------------------

// Walk the directory tree to find an inode
// Returns inode or NULLif not found
inode_t* vfs_resolve_path(const char* path);

// Find which filesystem owns this path
// Returns superblock or NULL if not found
superblock_t* vfs_find_mount(const char* path);

// --- File decriptor management ---------------------------------------------

// Get file struct for a file descriptor
// Returns NULL if fd invalid
file_t* vfs_get_file(int fd);

// Allocate a new file descriptor
// Returns fd number, -1 if table full
int vfs_alloc_fd(file_t* file);

// Free a file descriptor slot
void vfs_free_fd(int fd);

#endif