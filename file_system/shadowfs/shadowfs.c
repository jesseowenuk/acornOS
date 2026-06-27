#include <drivers/serial.h>
#include <file_system/shadowfs.h>
#include <file_system/vfs.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/core/string.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>

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
    // Guard against null superblock
    if(!sb)
    {
        kpanic("shadowfs_create_inode: null superblock!");
    }

    if(!sb->private_data)
    {
        kpanic("shadowfs_create_inode: null mount data!");
    }

    // Get mount private data so we can check quota and assign inode numbers
    shadowfs_mount_t* mount = (shadowfs_mount_t*)sb->private_data;

    // Step 1: allocate the VFS inode struct
    inode_t* inode = (inode_t*)kmalloc(sizeof(inode_t));
    kmemset(inode, 0, sizeof(inode_t));

    // Step 2: allocate private data
    // Files and directories have different private data layouts
    shadowfs_inode_t* priv = (shadowfs_inode_t*)kmalloc(sizeof(shadowfs_inode_t));
    kmemset(priv, 0, sizeof(shadowfs_inode_t));

    // Step 3: fill in the VFS inode fields

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

    // Step 4: - initialise private data based on type
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
    else
    {
        kpanic("shadowfs_create_inode: unknown inode type!");
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
        kserial_printf("shadowFS: quota %u exceeds 50%% of free RAM!\n", quota);
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

// --- shadowfs_create --------------------------------------------------
static inode_t* shadowfs_create(inode_t* dir, const char* name, uint32_t type)
{
    // Step 1: get mount data so we can check quota
    shadowfs_mount_t* mount = (shadowfs_mount_t*)dir->sb->private_data;

    // Step 2: check quota
    if(mount->used >= mount->quota)
    {
        kserial_printf("shadowFS: quota exceeded!\n");
        return 0;
    }

    // Step 3: get directory private data
    shadowfs_inode_t* dir_priv = (shadowfs_inode_t*)dir->private_data;

    if(!dir_priv)
    {
        return 0;
    }

    // Step 4: check name doesn't already exist
    shadowfs_dentry_t* entry = dir_priv->dir.entries;

    while(entry)
    {
        if(kstreq(entry->name, name))
        {
            kserial_printf("shadowFS: %s already exists!\n", name);
            return 0;
        }
        
        entry = entry->next;
    }

    // Step 5: create new inode
    inode_t* inode = shadowfs_create_inode(dir->sb, type);

    if(!inode)
    {
        return 0;
    }

    // Step 6: allocate directory entry
    shadowfs_dentry_t* dentry = (shadowfs_dentry_t*)kmalloc(sizeof(shadowfs_dentry_t));

    if(!dentry)
    {
        kfree(inode->private_data);
        kfree(inode);
        return 0;
    }

    // Step 7: fill in the directory entry
    kstrcpy(dentry->name, name, VFS_MAX_NAME);
    dentry->inode = inode;
    dentry->next = 0;

    // Step 8: Add to directory linked list
    if(!dir_priv->dir.entries)
    {
        // First entry
        dir_priv->dir.entries = dentry;
    }
    else
    {
        // Walk to the end of the list
        shadowfs_dentry_t* last = dir_priv->dir.entries;

        while(last->next)
        {
            last = last->next;
        }

        last->next = dentry;
    }

    dir_priv->dir.count++;

    // Step 9: update quota usage
    mount->used += sizeof(shadowfs_dentry_t) + sizeof(inode_t);

    kserial_printf("shadowFS: created %s\n", name);
    return inode;
}

static int shadowfs_delete(inode_t* dir, const char* name)
{
    if(!dir || !name)
    {
        return -1;
    }

    shadowfs_inode_t* dir_private = (shadowfs_inode_t*)dir->private_data;
    if(!dir_private)
    {
        return -1;
    }

    shadowfs_mount_t* mount = (shadowfs_mount_t*)dir->sb->private_data;
    if(!mount)
    {
        return -1;
    }

    // Walk linked list looking for the entry
    shadowfs_dentry_t* entry = dir_private->dir.entries;
    shadowfs_dentry_t* previous = 0;

    while(entry)
    {
        if(kstreq(entry->name, name))
        {
            // Found it - unlink from the list
            if(previous)
            {
                previous->next = entry->next;
            }
            else
            {
                dir_private->dir.entries = entry->next;
            }

            dir_private->dir.count--;

            // Free file data blocks if it's a file
            if(entry->inode->type == VFS_TYPE_FILE)
            {
                shadowfs_inode_t* inode_private = (shadowfs_inode_t*)entry->inode->private_data;
                shadowfs_block_t* block = inode_private->file.blocks;

                while(block)
                {
                    shadowfs_block_t* next = block->next;
                    // Free the data page back to PMM
                    pmm_free((void*)virtual_to_physical((uint64_t)block->data));
                    kfree(block);
                    block = next;
                }
            }

            // Free inode private data
            kfree(entry->inode->private_data);

            // Free inode
            kfree(entry->inode);

            // Update quota
            mount->used -= sizeof(shadowfs_dentry_t) + sizeof(inode_t);

            // Free directory entry
            kfree(entry);

            kserial_printf("shadowFS: deleted %s\n", name);
            return 0;
        }

        previous = entry;
        entry = entry->next;
    }

    kserial_printf("shadowFS: delete() %s not found!\n", name);
    return -1;
}

static int shadowfs_open(inode_t* inode, file_t* file)
{
    // No setup needed - VFS handles this as shadowFS is a RAM file system
    (void)inode;
    (void)file;
    return 0;
}

static int shadowfs_close(inode_t* inode, file_t* file)
{
    // No setup needed - VFS handles this as shadowFS is a RAM file system
    (void)inode;
    (void)file;
    return 0;
}

static int shadowfs_read(file_t* file, void* buffer, uint32_t size)
{
    if(!file || !buffer)
    {
        return -1;
    }

    if(!file->inode)
    {
        return -1;
    }

    shadowfs_inode_t* private = (shadowfs_inode_t*)file->inode->private_data;

    if(!private)
    {
        return -1;
    }

    if(size == 0)
    {
        return 0;
    }

    // Don't read past the end of the file
    uint32_t remaining = size;

    if(file->position + remaining > file->inode->size)
    {
        remaining = file->inode->size - file->position;
    }

    if(remaining == 0)
    {
        return 0;
    }

    uint8_t* dst = (uint8_t*)buffer;
    uint32_t read = 0;
    uint32_t offset = file->position;           // byte offset into file

    // Walk blocks to find starting position
    shadowfs_block_t* block = private->file.blocks;
    uint32_t block_start = 0;                   // byte offset of start of this block

    while(block && remaining > 0)
    {
        uint32_t block_end = block_start + block->used;

        // Is our read position within this block
        if(offset < block_end)
        {
            // Start reading from offset within this block
            uint32_t block_offset = offset - block_start;
            uint32_t available = block->used - block_offset;
            uint32_t to_read = (remaining < available) ? remaining : available;

            for(uint32_t i = 0; i < to_read; i++)
            {
                dst[i] = block->data[block_offset + i];
            }

            dst += to_read;
            read += to_read;
            offset += to_read;
            remaining -= to_read;
        }

        block_start += block->used;
        block = block->next;
    }

    file->position += read;

    return (int)read;
}

// --- shadowsfs_write --------------------------------------------------------
static int shadowfs_write(file_t* file, const void* buf, uint32_t size)
{
    if(!file)
    {
        kpanic("shadowfs_write: null file!");
    }

    if(!buf)
    {
        kpanic("shadowfs_write: null buffer!");
    }

    if(!file->inode)
    {
        kpanic("shadowfs_write: null inode");
    }

    inode_t* inode = file->inode;
    shadowfs_inode_t* priv = (shadowfs_inode_t*)inode->private_data;

    if(!priv)
    {
        kpanic("shadowfs_write: null private data!");
    }

    // Get mount data to check quota
    shadowfs_mount_t* mount = (shadowfs_mount_t*)inode->sb->private_data;

    if(!mount)
    {
        kpanic("shadowfs_write: null mount data!");
    }

    if(size == 0)
    {
        // Nothing to write
        return 0;
    }

    uint32_t remaining = size;
    const uint8_t* src = (const uint8_t*)buf;
    uint32_t written = 0;

    // Find the last block or start from scratch
    shadowfs_block_t* block = priv->file.blocks;
    shadowfs_block_t* last = 0;

    // Walk to the last block
    while(block)
    {
        last = block;
        block = block->next;
    }

    while(remaining > 0)
    {
        // Check quota before allocating
        if(mount->used >= mount->quota)
        {
            kserial_printf("shadowFS: quota exceeded on write!\n");

            // Return what we've written so far
            break;
        }

        // Do we need a new block
        if(!last || last->used == SHADOWFS_BLOCK_SIZE)
        {
            // Allocate a new block
            shadowfs_block_t* new_block = (shadowfs_block_t*)kmalloc(sizeof(shadowfs_block_t));

            if(!new_block)
            {
                kpanic("shadowfs_write: failed to allocate block!");
            }

            // 4KB page from PMM - plenty of those
            new_block->data = (uint8_t*)physical_to_virtual((uint64_t)pmm_alloc());

            new_block->used = 0;
            new_block->next = 0;

            // Link into the chain
            if(!priv->file.blocks)
            {
                // First block
                priv->file.blocks = new_block;
            }
            else
            {
                // Append to chain
                last->next = new_block;
            }

            last = new_block;
            priv->file.block_count++;

            // Update quota
            mount->used += sizeof(shadowfs_block_t);
        }

        // How much space in current block
        uint32_t space = SHADOWFS_BLOCK_SIZE - last->used;
        uint32_t to_write = (remaining < space) ? remaining : space;

        // Copy data into block
        for(uint32_t i = 0; i < to_write; i++)
        {
            last->data[last->used + i] = src[i];
        }

        last->used += to_write;
        src += to_write;
        written += to_write;
        remaining -= to_write;
    }

    // Update inode size and position
    inode->size += written;
    file->position += written;

    return (int)written;
}

static int shadowfs_readdir(file_t* file, dentry_t* dentry)
{
    if(!file || !dentry)
    {
        return -1;
    }

    if(!file->inode)
    {
        return -1;
    }

    shadowfs_inode_t* private = (shadowfs_inode_t*)file->inode->private_data;

    if(!private)
    {
        return -1;
    }

    // Walk to the nth entry where n = file->position
    shadowfs_dentry_t* entry = private->dir.entries;
    uint32_t index = 0;

    while(entry)
    {
        if(index == file->position)
        {
            // Found the entry at this position
            kstrcpy(dentry->name, entry->name, VFS_MAX_NAME);
            dentry->inode = entry->inode;
            dentry->type = entry->inode->type;

            // Advance position for next call
            file->position++;
            return 1;           // 1 = entry returned
        }

        index++;
        entry = entry->next;
    }

    // 0 = no more entries
    return 0;
}

static int shadowfs_mkdir(inode_t* dir, const char* name)
{
    if(!dir || !name)
    {
        return -1;
    }

    // Use shadowfs_create to create a directory inode
    inode_t* new_dir = shadowfs_create(dir, name, VFS_TYPE_DIR);

    if(!new_dir)
    {
        kserial_printf("shadowFS: mkdir failed for %s\n", name);
        return -1;
    }

    kserial_printf("shadowFS: mkdir %s\n", name);

    return 0;
}

static int shadowfs_truncate(inode_t* inode, uint32_t size)
{
    if(!inode)
    {
        return -1;
    }

    shadowfs_inode_t* private = (shadowfs_inode_t*)inode->private_data;
    if(!private)
    {
        return -1;
    }

    shadowfs_mount_t* mount = (shadowfs_mount_t*)inode->sb->private_data;
    if(!mount)
    {
        return -1;
    }

    // Only files can be truncated
    if(inode->type != VFS_TYPE_FILE)
    {
        return -1;
    }

    if(size == 0)
    {
        // Free all blocks
        shadowfs_block_t* block = private->file.blocks;

        while(block)
        {
            shadowfs_block_t* next = block->next;
            pmm_free((void*)virtual_to_physical((uint64_t)block->data));
            mount->used -= sizeof(shadowfs_block_t);
            kfree(block);
            block = next;
        }

        private->file.blocks = 0;
        private->file.block_count = 0;
        inode->size = 0;

        kserial_printf("shadowFS: truncated to 0\n");
        return 0;
    }

    // Truncate to specific size - free blocks beyond size
    uint32_t bytes_remaining = size;
    shadowfs_block_t* block = private->file.blocks;
    shadowfs_block_t* previous = 0;

    while(block)
    {
        if(bytes_remaining == 0)
        {
            // Free this block and all following
            shadowfs_block_t* next = block->next;
            pmm_free((void*)virtual_to_physical((uint64_t)block->data));
            mount->used -= sizeof(shadowfs_block_t);
            kfree(block);

            private->file.block_count--;
            if(previous)
            {
                previous->next = 0;
            }

            block = next;
        }
        else if(bytes_remaining < block->used)
        {
            // Partial block - trim it
            block->used = bytes_remaining;
            bytes_remaining = 0;
            previous = block;
            block = block->next;
        }
        else
        {
            bytes_remaining -= block->used;
            previous = block;
            block = block->next;
        }
    }

    inode->size = size;
    kserial_printf("shadowFS: truncated to %u bytes\n", size);
    return 0;
}

void shadowfs_stats()
{
    // Walk mount table to find shadowFS mounts
    // For now just print /temp stats
    superblock_t* sb = vfs_find_mount("/temp");
    if(!sb)
    {
        return;
    }

    shadowfs_mount_t* mount = (shadowfs_mount_t*)sb->private_data;
    if(!mount)
    {
        return;
    }

    kserial_printf("shadowFS /temp: used=%uKB quota=%uKB\n", mount->used / 1024, mount->quota / 1024, (mount->quota - mount->used) / 1024);
}