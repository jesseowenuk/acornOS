#include <file_system/procfs.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/core/string.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/processes/process.h>

// --- Forward declarations ---------------------------------
static inode_t* procfs_lookup(inode_t* dir, const char* name);
static int procfs_open(inode_t* inode, file_t* file);
static int procfs_close(inode_t* inode, file_t* file);
static int procfs_read(file_t* file, void* buffer, uint32_t size);
static int procfs_readdir(file_t* file, dentry_t* dentry);

// --- Operations table -------------------------------------
static fs_ops_t procfs_ops =
{
    .lookup = procfs_lookup,
    .open = procfs_open,
    .close = procfs_close,
    .read = procfs_read,
    .readdir = procfs_readdir,
    .create = 0,                    // procFS is read-only
    .write = 0,
    .delete = 0,
    .mkdir = 0,
    .rmdir = 0,
    .seek = 0,
    .truncate = 0,
};

fs_ops_t* procfs_get_ops()
{
    return &procfs_ops;
}

// --- Create a procFS inode ----------------------------------------
static inode_t* procfs_create_inode(superblock_t* sb, uint32_t type,
                                    procfs_inode_type_t procfs_type,
                                    uint64_t pid, const char* name)
{
    inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));
    kmemset(inode, 0, sizeof(inode_t));

    procfs_inode_t* private = (procfs_inode_t*)kmalloc(sizeof(procfs_inode_t));
    kmemset(private, 0, sizeof(procfs_inode_t));

    private->type = procfs_type;
    private->pid = pid;
    private->name = name;

    inode->type = type;
    inode->size = 0;
    inode->permissions = 0444;          // Read-only
    inode->link_count = 1;
    inode->ops = &procfs_ops;
    inode->sb = sb;
    inode->private_data = private;

    return inode;
}

// --- procfs_mount ---------------------------------------------
int procfs_mount(const char* path)
{
    int result = vfs_mount(path, &procfs_ops, 0);
    if(result < 0)
    {
        kserial_printf("procFS: vfs_mount failed!\n");
        return -1;
    }

    superblock_t* sb = vfs_find_mount(path);
    if(!sb)
    {
        kserial_printf("procFS: could not find superblock!\n");
        return -1;
    }

    // Create root inode
    inode_t* root = procfs_create_inode(sb, VFS_TYPE_DIR, PROCFS_ROOT, 0, "process");

    if(!root)
    {
        kserial_printf("procFS: failed to create root inode!\n");
        return -1;
    }

    sb->root = root;

    kserial_printf("procFS: mounted at %s\n", path);
    return 0;
}

// --- procfs_lookup -----------------------------------------------------
static inode_t* procfs_lookup(inode_t* dir, const char* name)
{
    kserial_printf("procfs_lookup: name=%s\n", name);

    if(!dir)
    {
        return 0;
    }

    procfs_inode_t* private = (procfs_inode_t*)dir->private_data;
    if(!private)
    {
        return 0;
    }

    superblock_t* sb = dir->sb;

    if(private->type == PROCFS_ROOT)
    {
        kserial_printf("procfs_lookup: searching for pid=%s in process table\n", name);

        // Looking up in /process - could be meminfo, mounts or a PID
        if(kstreq(name, "meminfo"))
        {
            return procfs_create_inode(sb, VFS_TYPE_FILE, PROCFS_INFO_FILE, 0, "meminfo");
        }

        if(kstreq(name, "mounts"))
        {
            return procfs_create_inode(sb, VFS_TYPE_FILE, PROCFS_INFO_FILE, 0, "mounts");
        }

        // Try to parse as a PID number
        uint64_t pid = 0;
        const char* p = name;
        
        while(*p >= '0' && *p <= '9')
        {
            pid = pid * 10 + (*p++ - '0');
        }

        kserial_printf("procfs_lookup: parsed pid=%lu\n", pid);

        // Must have consumed all chars and be a valid PID
        if(*p != 0)
        {
            return 0;
        }

        // Check PID exists in process table
        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            if(process_table[i])
            {
                kserial_printf("procfs_lookup: table[%d] pid=%lu\n", i, process_table[i]->pid);
            }

            if(process_table[i] && process_table[i]->pid == pid)
            {
                kserial_printf("procfs_lookup: found pid=%lu\n", pid);
                return procfs_create_inode(sb, VFS_TYPE_DIR, PROCFS_PID_DIR, pid, name);
            }
        }

        kserial_printf("procfs_lookup: pid=%lu not found!\n", pid);

        return 0;
    }

    if(private->type == PROCFS_PID_DIR)
    {
        // Looking up in /process/[pid] - status, memory, files
        if(kstreq(name, "status"))
        {
            return procfs_create_inode(sb, VFS_TYPE_FILE, PROCFS_PID_FILE, private->pid, "status");
        }

        if(kstreq(name, "memory"))
        {
            return procfs_create_inode(sb, VFS_TYPE_FILE, PROCFS_PID_FILE, private->pid, "memory");
        }

        if(kstreq(name, "files"))
        {
            return procfs_create_inode(sb, VFS_TYPE_FILE, PROCFS_PID_FILE, private->pid, "files");
        }

        return 0;
    }

    return 0;
}

// --- procfs_readdir -----------------------------------------------
static int procfs_readdir(file_t* file, dentry_t* dentry)
{
    if(!file || !dentry)
    {
        return -1;
    }

    procfs_inode_t* private = (procfs_inode_t*)file->inode->private_data;

    if(private->type == PROCFS_ROOT)
    {
        // List /process - info files first, then one entry per process
        // Positions 0,1 = meminfo, mounts
        // Positions 2+ = PIDs
        uint32_t position = file->position;

        if(position == 0)
        {
            kstrcpy(dentry->name, "meminfo", VFS_MAX_NAME);
            dentry->type = VFS_TYPE_FILE;
            file->position++;
            return 1;
        }

        if(position == 1)
        {
            kstrcpy(dentry->name, "mounts", VFS_MAX_NAME);
            dentry->type = VFS_TYPE_FILE;
            file->position++;
            return 1;
        }

        // Walk process table for PIDs
        uint32_t pid_index = position - 2;
        uint32_t found = 0;
        
        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            if(!process_table[i])
            {
                continue;
            }

            if(found == pid_index)
            {
                // Convert PID to string
                uint64_t pid = process_table[i]->pid;
                char* p = dentry->name;

                if(pid == 0)
                {
                    *p++ = '0';
                    *p = 0;
                }
                else
                {
                    char temp[20];
                    int length = 0;

                    while(pid > 0)
                    {
                        temp[length++] = '0' + (pid % 10);
                        pid /= 10;
                    }

                    for(int j = length - 1; j >= 0; j--)
                    {
                        *p++ = temp[j];
                    }

                    *p = 0;
                }

                dentry->type = VFS_TYPE_DIR;
                file->position++;
                return 1;
            }

            found++;
        }

        // End of directory
        return 0;
    }

    if(private->type == PROCFS_PID_DIR)
    {
        // List /process/[pid] - status, memory files
        static const char* pid_files[] = { "status", "memory", "files", 0 };

        uint32_t position = file->position;

        if(!pid_files[position])
        {
            return 0;
        }

        kstrcpy(dentry->name, pid_files[position], VFS_MAX_NAME);
        dentry->type = VFS_TYPE_FILE;
        file->position++;
        return 1;
    }

    return 0;
}

// --- procfs_open -----------------------------------------------------
static int procfs_open(inode_t* inode, file_t* file)
{
    // Nothing to do - content generated on read
    (void)inode;
    (void)file;
    return 0;
}

// --- procfs_close ----------------------------------------------------
static int procfs_close(inode_t* inode, file_t* file)
{
    (void)inode;
    (void)file;
    return 0;
}

// --- procfs_read -----------------------------------------------------
static int procfs_read(file_t* file, void* buffer, uint32_t size)
{
    if(!file || !buffer)
    {
        return -1;
    }

    procfs_inode_t* private = (procfs_inode_t*)file->inode->private_data;
    if(!private)
    {
        return -1;
    }

    // Generate content into temporary buffer
    char temp[512];
    int length = 0;

    if(private->type == PROCFS_INFO_FILE)
    {
        if(kstreq(private->name, "meminfo"))
        {
            uint64_t free_pages = pmm_free_pages();
            uint64_t used_pages = pmm_used_pages();
            uint64_t total_pages = free_pages + used_pages;

            length = ksnprintf(temp, sizeof(temp),
                "MemTotal:  %lu KB\n"
                "MemFree:   %lu KB\n"
                "MemUsed:   %lu KB\n",
                total_pages * 4,
                free_pages * 4,
                used_pages * 4);
        }
        else if(kstreq(private->name, "mounts"))
        {
            // TODO: walkn VFS mount table
            length = ksnprintf(temp, sizeof(temp),
                "/temp        shadowFS\n"
                "/devices     devFS\n"
                "/process     procFS\n");
        }
    }
    else if(private->type == PROCFS_PID_FILE)
    {
        // Find the process
        process_t* process = 0;

        for(int i = 0; i < MAX_PROCESSES; i++)
        {
            if(process_table[i] && process_table[i]->pid == private->pid)
            {
                process = process_table[i];
                break;
            }
        }

        if(!process)
        {
            length = ksnprintf(temp, sizeof(temp), "Process not found\n");
        }
        else if(kstreq(private->name, "status"))
        {
            const char* state = "unknown";

            switch(process->state)
            {
                case PROCESS_READY:
                {
                    state = "ready";
                    break;
                }

                case PROCESS_RUNNING:
                {
                    state = "running";
                    break;
                }

                case PROCESS_BLOCKED:
                {
                    state = "blocked";
                    break;
                }

                case PROCESS_DEAD:
                {
                    state = "dead";
                    break;
                }

                case PROCESS_SLEEPING:
                {
                    state = "sleeping";
                    break;
                }
            }

            length = ksnprintf(temp, sizeof(temp),
                "Name:      %s\n"
                "PID:       %lu\n"
                "Parent:    %lu\n"
                "State:     %s\n",
                process->name,
                process->pid,
                process->parent_pid,
                state);
        }
        else if(kstreq(private->name, "memory"))
        {
            length = ksnprintf(temp, sizeof(temp),
                "Stack:     0x%lx\n"
                "StackTop:  0x%lx\n",
                process->stack,
                process->stack_top);
        }
        else if(kstreq(private->name, "files"))
        {
            // TODO: per-process file descriptor table
            length = ksnprintf(temp, sizeof(temp), "fd table not yet per-process\n");
        }
    }

    // Copy from temp into buffer respecting position and size
    if(file->position >= (uint32_t)length)
    {
        // EOF
        return 0;
    }

    uint32_t available = length - file->position;
    uint32_t to_copy = (size < available) ? size : available;

    uint8_t* dst = (uint8_t*)buffer;

    for(uint32_t i = 0; i < to_copy; i++)
    {
        dst[i] = temp[file->position + i];
    }

    file->position += to_copy;

    return (int)to_copy;
}