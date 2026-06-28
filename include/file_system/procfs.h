#ifndef PROCFS_H
#define PROCFS_H

#include <file_system/vfs.h>

#include <stdint.h>

// --- Inode types --------------------------------------------
// procFS has three kinds of inodes:
//      root - /process itself
//      pid_dir - /process/[pid]
//      pid_file - /process/[pid]/status etc.

typedef enum
{
    PROCFS_ROOT,            // /process
    PROCFS_PID_DIR,         // /process/[pid]
    PROCFS_PID_FILE,        // /process/[pid]/status etc.
    PROCFS_INFO_FILE,       // /process/meminfo, /process/mounts
} procfs_inode_type_t;

// --- Inode private data -------------------------------------
typedef struct
{
    procfs_inode_type_t type;   // What kind of procFS inode is this?
    uint64_t pid;               // PID this node belongs to (0 = not a pid node)
    const char* name;           // File name within pid dir
} procfs_inode_t;

// --- Functions -----------------------------------------------
int procfs_mount(const char* path);
fs_ops_t* procfs_get_ops();

#endif