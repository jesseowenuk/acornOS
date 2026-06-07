#ifndef SHADOWFS_H
#define SHADOWFS_H

#include <stdint.h>
#include "vfs.h"                // For inode_t, file_t, dentry_t, fs_ops_t

// --- shadowFS ----------------------------------------------------
// A RAM based filesystem for acornOS
// Data lives entirely in memory - lost when power is removed
// Like a shadow that disappears when the sun (power) goes away
//
// Used for:
//  /temp           - temporary files
//  /devices        - device files
//  /process        - process information
//  /system         - system information

// --- Constants -----------------------------------------------------

// 4KB blocks - matches PMM page size
// Natural fit with memory manager
// Efficient allocation and tracking
#define SHADOWFS_BLOCK_SIZE     4096

// Maximum file size: 16MB
// Prevents single file hogging quota
#define SHADOWFS_MAX_FILESIZE   (16 * 1024 *1024)

// --- Block ---------------------------------------------------------
// A single 4KB blockof file data
// Files are stored as a linked list of blocks

typedef struct shadowfs_block
{
    // The actual file data - 4096 bytes per block
    uint8_t data[SHADOWFS_BLOCK_SIZE];

    // Bytes used in this block, last block may not be full.
    // 0 = block is empty
    uint32_t used;

    // Next block in the chain
    // NULL = this is the last block
    struct shadowfs_block* next;
} shadowfs_block_t;

// --- Directory entry ---------------------------------------------------
// One entry in a directory
// Maps a filename to an inode
// Stored as a linked list within each directory

typedef struct shadowfs_dentry
{
    // Filename - not the full path
    // e.g "notes.txt" not "/tmp/notes.txt"
    char name[VFS_MAX_NAME];

    // VFS inode this name points to
    inode_t* inode;

    // Next entry in this directory
    // NULL = last entry
    struct shadowfs_dentry* next;
} shadowfs_dentry_t;

// --- Inode private data ------------------------------------------------
// Stored in inode->private_data
// Different layout for files vs directories

typedef struct shadowfs_inode
{
    union
    {
        struct
        {
            // First block of file data
            // NULL = empty file
            shadowfs_block_t* blocks;

            // Number of blocks allocated
            // used when inode->type == VFS_TYPE_FILE
            uint32_t block_count;
        } file;

        struct 
        {
            // First directory entry
            // NULL = empty directory
            shadowfs_dentry_t* entries;

            // Number of entries 
            // Used when inode->type == VFS_TYPE_DIR
            uint32_t count;
        } dir;
    };
} shadowfs_inode_t;

// --- Mount private data -------------------------------------------
// Stored in superblock->private_data
// One per mounted shadowFS instance

typedef struct shadowfs_mount
{
    // Maximum bytes this mount can use, set at mount time
    // e.g. 8MB for /temp
    uint32_t quota;

    // Bytes currently in use including block overhead
    uint32_t used;

    // Next inode number to assign, incremented on each creation
    uint32_t next_inode_num;

    // Root directory of this mount
    inode_t* root_inode;
} shadowfs_mount_t;

// --- Function -----------------------------------------------------

// Iniitalise shadowFS and return operations table
// Called once - returns the ops table to pass to vfs_mount()
fs_ops_t* shadowfs_get_ops();

// Mount a shadowFS instance at the given path
// quota = maximum bytes this instance can use
// Returns 0 on success, -1 on failure
int shadowfs_mount(const char* path, uint32_t quota);

// Print statistics for all mounted shadowFS instances
void shadowfs_stats();

#endif