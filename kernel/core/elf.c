#include <kernel/core/elf.h>
#include <kernel/core/kprintf.h>
#include <kernel/core/panic.h>
#include <kernel/memory/pmm.h>
#include <kernel/paging.h>

// --- elf_load -----------------------------------------
// Loads an ELF64 binary already present in physical memory
// physical_address = physical address where the ELF file starts
// Returns entry point virtual address, or 0 on failure
uint64_t elf_load(uint64_t physical_address)
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