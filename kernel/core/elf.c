#include <kernel/core/elf.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>

// Get the elf entry point
uint64_t elf_get_entry(uint64_t physical_address)
{
 // Access the ELF via the direct physical map
    uint8_t* file = (uint8_t*)physical_to_virtual(physical_address);
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

    kserial_printf("elf_load: entry=0x%lx phnum=%u\n", header->e_entry, header->e_phnum);

    return header->e_entry;
}

// --- elf_load -----------------------------------------
// Loads an ELF64 binary already present in physical memory
// physical_address = physical address where the ELF file starts
// Returns entry point virtual address, or 0 on failure
uint64_t elf_load(uint64_t physical_address, process_t* process)
{
    // Access the ELF via the direct physical map
    uint8_t* file = (uint8_t*)physical_to_virtual(physical_address);
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

    for(uint16_t i = 0; i < header->e_phnum; i++)
    {
        elf64_phdr_t* ph = &pheaders[i];

        // Skip non-loadable segments
        if(ph->p_type != PT_LOAD)
        {
            continue;
        }

        kserial_printf("elf_load: LOAD segment %u vaddr=0x%lx filesz=%lu memsz=%lu\n", i, ph->p_vaddr, ph->p_filesz, ph->p_memsz);

        // Calculate page flags
        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if(ph->p_flags & PF_W)
        {
            flags |= PAGE_WRITABLE;
        }

        kserial_printf("segment: offset=0x%lx vaddress=0x%lx filesz=%lu memsz=%lu\n", ph->p_offset, ph->p_vaddr, ph->p_filesz, ph->p_memsz);

        // Allocate and map pages for this segment
        uint64_t virtual_address = ph->p_vaddr & ~0xFFFUL;          // Align down to a page
        uint64_t virtual_address_end = ((ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFUL) + PAGE_SIZE;

        kserial_printf("va=0x%lx va_end=0x%lx\n", virtual_address, virtual_address_end);

        for(uint64_t va = virtual_address; va < virtual_address_end; va += PAGE_SIZE)
        {
            uint64_t physical = (uint64_t)pmm_alloc();
            kserial_printf("elf_load: mapping va=0x%lx -> physical=0x%lx\n", va, physical);
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

    // Update process CPU state
    process->cpu.rip = header->e_entry;
    process->cpu.rsp = stack_virtual + PAGE_SIZE - 8;
    process->cpu.rflags = 0x200;                // Interrupts enabled
    process->cpu.cs = 0x08;                     // kernel code for now
    process->cpu.ds = 0x10;
    process->cpu.ss = 0x10;
    
    kserial_printf("elf_load: loaded OK entry=0x%lx stack=0x%lx\n", header->e_entry, process->cpu.rsp);

    return header->e_entry;
}