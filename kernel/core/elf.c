#include <file_system/vfs.h>
#include <kernel/core/elf.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/mem.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>

extern void iret_to_usermode();

// Most bytes of argv strings + the pointer array allowed to fit onto
// the single-page user stack elf_load() builds. Generous for shell
// command arguments; enforced explicitly rather than silently
// overflowing into the program's own stack usage.
#define MAX_ARGV_BLOCK_SIZE 2048

// Most argv entries accepted so string_addr[] below can be a fixed
// size array instead of a variable-length one.
#define MAX_ARGV_COUNT 32

// --- build_argv_stack ------------------------------------------------
// Writes argv's strings and a NULL-terminated pointer array onto the
// top of a process's freshly mapped, single-page user stack - via the
// kernel's physical direct map, the same technique elf_load() already
// uses to populate segments before the new page directory is active.
//
// argv[0] is conventionally the program's own namel setting that is
// the caller's job, not this function's - it just copies whatever
// NULL-terminated array it's given.
//
// On success, *out_argc and *out_stack_top (the address of the argv
// pointer array itself, which also becomes the process's initial RSP -
// its own stack usage grows down from there) are filled in.
// Returns 1 on success, 0 on failure (no argv given, too many
// arguments, or the block doesn't fit in MAX_ARGV_BLOCK_SIZE)
static int build_argv_stack(page_directory_t* page_dir, uint64_t stack_virtual, char** argv, int* out_argc, uint64_t* out_stack_top)
{
    if(!argv || !argv[0])
    {
        return 0;
    }

    int argc = 0;

    while(argv[argc])
    {
        argc++;

        if(argc >= MAX_ARGV_COUNT)
        {
            kserial_printf("build_argv_stack: too many arguments, dropping argv\n");
            return 0;
        }
    }

    uint64_t page_top = stack_virtual + PAGE_SIZE;
    uint8_t* page_base = (uint8_t*)physical_to_virtual(get_physical_in(page_dir, stack_virtual));
    uint64_t cursor = page_top;
    uint64_t string_addr[MAX_ARGV_COUNT];

    // Place strings from the top of the page downward, recording each
    // one's final user-space address for the pointer array that follows.
    for(int i = argc - 1; i >= 0; i--)
    {
        uint64_t len = 0;

        while(argv[i][len])
        {
            len++;
        }

        len++;                      // include the null terminator

        if(page_top - (cursor - len) > MAX_ARGV_BLOCK_SIZE)
        {
            kserial_printf("build_argv_stack: argv too large, dropping argv\n");
            return 0;
        }

        cursor -= len;

        for(uint64_t j = 0; j < len; j++)
        {
            page_base[(cursor & 0xFFF) + j] = argv[i][j];
        }

        string_addr[i] = cursor;
    }

    // Pointer array sits just below the strings, 8-byte aligned, argc+1
    // entries (argv[argc] = NULL - the standard convention, even though
    // there's no envp/auxv here yet
    cursor &= ~0x7UL;
    uint64_t pointer_array_bytes = (uint64_t)(argc + 1) * 8;

    if(page_top - (cursor - pointer_array_bytes) > MAX_ARGV_BLOCK_SIZE)
    {
        kserial_printf("build_argv_stack: argv too large, dropping argv\n");
        return 0;
    }

    cursor -= pointer_array_bytes;

    uint64_t* pointer_array = (uint64_t*)(page_base + (cursor & 0xFFF));

    for(int i = 0; i < argc; i++)
    {
        pointer_array[i] = string_addr[i];
    }

    pointer_array[argc] = 0;

    *out_argc = argc;
    *out_stack_top = cursor;

    return 1;
}

// Get the elf entry point
uint64_t elf_get_entry(uint8_t* data)
{
    elf64_header_t* header = (elf64_header_t*)data;

    // Verify magic bytes: 0x7F 'E' 'L' 'F'
    if(header->e_ident[0] != 0x7F ||
       header->e_ident[1] != 'E' ||
       header->e_ident[2] != 'L' ||
       header->e_ident[3] != 'F')
    {
        kserial_printf("elf_load: bad magic!\n");
        return 0;
    }

    // Verify 64-bit, little endian, x86_64 executable
    if(header->e_ident[4] != ELF_CLASS_64)
    {
        kserial_printf("elf_load: not 64-bit!\n");
        return 0;
    }

    if(header->e_machine != ELF_MACHINE_X86_64)
    {
        kserial_printf("elf_load: not x86_64!\n");
        return 0;
    }

    if(header->e_type != ELF_TYPE_EXEC)
    {
        kserial_printf("elf_load: not executable!\n");
        return 0;
    }

    kserial_printf("elf_load: entry=0x%lx phnum=%u\n", header->e_entry, header->e_phnum);

    return header->e_entry;
}

// --- elf_load -----------------------------------------
// Loads an ELF64 binary already present in physical memory
// physical_address = physical address where the ELF file starts
// Returns entry point virtual address, or 0 on failure
uint64_t elf_load(uint8_t* data, process_t* process, char** argv)
{
    // Access the ELF via the direct physical map
    uint8_t* file = data;
    elf64_header_t* header = (elf64_header_t*)file;

    // Verify magic bytes: 0x7F 'E' 'L' 'F'
    if(header->e_ident[0] != 0x7F ||
       header->e_ident[1] != 'E' ||
       header->e_ident[2] != 'L' ||
       header->e_ident[3] != 'F')
    {
        kserial_printf("elf_load: bad magic!\n");
        return 0;
    }

    // Verify 64-bit, little endian, x86_64 executable
    if(header->e_ident[4] != ELF_CLASS_64)
    {
        kserial_printf("elf_load: not 64-bit!\n");
        return 0;
    }

    if(header->e_machine != ELF_MACHINE_X86_64)
    {
        kserial_printf("elf_load: not x86_64!\n");
        return 0;
    }

    if(header->e_type != ELF_TYPE_EXEC)
    {
        kserial_printf("elf_load: not executable!\n");
        return 0;
    }

    // Walk program headers looking for PT_LOAD segments
    elf64_phdr_t* pheaders = (elf64_phdr_t*)(file + header->e_phoff);

    // Tracks the highest address touched by any LOAD segment - the heap
    // starts right after this.
    uint64_t heap_start = 0;

    for(uint16_t i = 0; i < header->e_phnum; i++)
    {
        elf64_phdr_t* ph = &pheaders[i];

        // Skip non-loadable segments
        if(ph->p_type != PT_LOAD)
        {
            continue;
        }

        // Page flags
        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if(ph->p_flags & PF_W)
        {
            flags |= PAGE_WRITABLE;
        }

        // Align to page boundaries
        uint64_t virtual_address = ph->p_vaddr & ~0xFFFUL;          // Align down to a page
        uint64_t virtual_address_end = ((ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFUL);

        if(virtual_address_end > heap_start)
        {
            heap_start = virtual_address_end;
        }

        // Allocate and map pages, zeroing each via direct map
        for(uint64_t va = virtual_address; va < virtual_address_end; va += PAGE_SIZE)
        {
            uint64_t physical = (uint64_t)pmm_alloc();
            map_page_in(process->page_dir, va, physical, flags);

            // Zero all allocated pages first via direct map
            uint8_t* page = (uint8_t*)physical_to_virtual(physical);
            for(int k = 0; k < PAGE_SIZE; k++)
            {
                page[k] = 0;
            }
        }

        // Copy file data into mapped memory
        uint8_t* src = file + ph->p_offset;
        uint64_t remaining = ph->p_filesz;
        uint64_t copy_va = ph->p_vaddr;

        while(remaining > 0)
        {
            uint64_t page_offset = copy_va & 0xFFF;
            uint64_t physical = get_physical_in(process->page_dir, copy_va & ~0xFFFUL);
            uint8_t* dst = (uint8_t*)physical_to_virtual(physical) + page_offset;
            uint64_t to_copy = PAGE_SIZE - page_offset;

            if(to_copy > remaining)
            {
                to_copy = remaining;
            }

            for(uint64_t j = 0; j < to_copy; j++)
            {
                dst[j] = src[j];
            }

            src += to_copy;
            copy_va += to_copy;
            remaining -= to_copy;
        }

        // Zero BSS region (memsz - filesz) page by page via direct map
        uint64_t bss_va = ph->p_vaddr + ph->p_filesz;
        uint64_t bss_end = ph->p_vaddr + ph->p_memsz;

        while(bss_va < bss_end)
        {
            uint64_t page_offset = bss_va & 0xFFF;
            uint64_t physical = get_physical_in(process->page_dir, bss_va & ~0xFFFUL);
            uint8_t* dst = (uint8_t*)physical_to_virtual(physical) + page_offset;
            uint64_t to_zero = PAGE_SIZE - page_offset;

            if(to_zero > bss_end - bss_va)
            {
                to_zero = bss_end - bss_va;
            }

            for(uint64_t j = 0; j < to_zero; j++)
            {
                dst[j] = 0;
            }

            bss_va += to_zero;
        }
    }

    // Allocate and map user stack
    uint64_t stack_physical = (uint64_t)pmm_alloc();
    uint64_t stack_virtual = 0x00007FFFFFFFE000UL;

    map_page_in(process->page_dir,
                stack_virtual,
                stack_physical,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    // Zero stack
    uint8_t* stack_page = (uint8_t*)physical_to_virtual(stack_physical);
    for(int k = 0; k < PAGE_SIZE; k++)
    {
        stack_page[k] = 0;
    }

    // User stack top - argc/argv (if any) get written onto this same
    // page first, and the true starting RSP moves down below them
    int argc = 0;
    uint64_t user_stack_top = stack_virtual + PAGE_SIZE - 8;
    uint64_t argv_user_addr = 0;

    if(build_argv_stack(process->page_dir, stack_virtual, argv, &argc, &user_stack_top))
    {
        argv_user_addr = user_stack_top;
    }

    // Set up iret frame on kernel stack for ring 3 entry
    // switch_context will use iret_to_usermode to enter ring 3
    uint64_t* kstack = (uint64_t*)process->stack_top;
    *kstack-- = 0x23;                               // SS - user stack segment
    *kstack-- = user_stack_top;                     // RSP - user stack pointer
    *kstack-- = 0x200;                              // RFLAGS - interrupts enabled
    *kstack-- = 0x2B;                               // CS - user code segment
    *kstack-- = header->e_entry;                    // RIP - program entry point

    // Update process CPU state
    // cpu.rsp points to iret frame, cpu.rip = iret_to_usermode
    process->stack_top = (uint64_t)(kstack + 1);
    process->cpu.rsp = process->stack_top;
    process->cpu.rip = process->stack_top;
    process->cpu.rflags = 0x200;                // Interrupts enabled
    process->cpu.cs = 0x2B;                     // User code segment (ring 3)
    process->cpu.ds = 0x10;                     // User data segment (ring 3)
    process->cpu.ss = 0x10;                     // User stack segment (ring 3)
    process->is_user = 1;                       // Permanently a ring 3 process
    process->heap_start = heap_start;           // Heap starts empty, right after
    process->heap_end = heap_start;             // the last LOAD segment (page-aligned)

    // enter_ring3()/iret_to_usermode restore RDI/RSI from cpu state right
    // before iret - so _start(argc, argv) recieves them exactly as if it
    // had been called normally, no special stack based ABI needed
    process->cpu.rdi = (uint64_t)argc;
    process->cpu.rsi = argv_user_addr;

    kserial_printf("elf_load: loaded OK entry=0x%lx stack=0x%lx\n", header->e_entry, process->cpu.rsp);

    return header->e_entry;
}

// --- elf_load_from_path -----------------------------------------------
// Reads a whole ELF file from the VFS into a temorary heap buffer,
// then hands it to elf_load. The buffer is only staging - elf_load()
// copies segment data out of it into the process's own pages, so it's
// safe to free once elf_load() returns.
uint64_t elf_load_from_path(const char* path, process_t* process, char** argv)
{
    int fd = vfs_open(path, O_RDONLY);
    if(fd < 0)
    {
        kserial_printf("elf_load_from_path: could not open '%s'\n", path);
        return 0;
    }

    file_t* file = vfs_get_file(fd);
    if(!file || !file->inode || file->inode->size == 0)
    {
        kserial_printf("elf_load from path: '%s' has no size\n", path);
        vfs_close(fd);
        return 0;
    }

    uint32_t size = file->inode->size;
    uint8_t* buffer = (uint8_t*)kmalloc(size);
    if(!buffer)
    {
        kserial_printf("elf_load_from_path: out of memory reading '%s'\n", path);
        vfs_close(fd);
        return 0;
    }

    int bytes_read = vfs_read(fd, buffer, size);
    vfs_close(fd);

    if(bytes_read < 0 || (uint32_t)bytes_read != size)
    {
        kserial_printf("elf_load_from_path: short read '%s'\n", path);
        kfree(buffer);
        return 0;
    }

    uint64_t entry = elf_load(buffer, process, argv);

    kfree(buffer);

    return entry;
}