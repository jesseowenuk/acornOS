#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <stdint.h>

// TEMPORARY: hello.elf is loaded by stage 2 to this fixed physical address
// Will be removed once we have a persistent file system (barkFS) and 
// load ELF binaries from disk via the VFS instead
#define HELLO_ELF_PHYSICAL_ADDRESS 0x300000

// --- ELF64 magic --------------------------------------
#define ELF_MAGIC           0x464C457F          // \x7fELF in little endian
#define ELF_CLASS_64        2                   // 64-bit
#define ELF_DATA_LSB        1                   // Little endian
#define ELF_TYPE_EXEC       2                   // Executable
#define ELF_MACHINE_X86_64  0x3E                // x86_64

// --- Program header types ------------------------------
#define PT_NULL             0                   // Unused
#define PT_LOAD             1                   // Loadable segment
#define PT_DYNAMIC          2                   // Dynamic linking
#define PT_INTERP           3                   // Interpreter path

// --- Program header flags -------------------------------
#define PF_X                0x1                 // Execute
#define PF_W                0x2                 // Write
#define PF_R                0x4                 // Read

// --- ELF64 header ---------------------------------------
typedef struct __attribute__((packed))
{
    uint8_t e_ident[16];                        // Magic, class, data, version, OS/ABI
    uint16_t e_type;                            // Object file type (ET_EXEC = 2)
    uint16_t e_machine;                         // Architecture (EM_X86_64 = 0x3E)
    uint32_t e_version;                         // ELF version (always 1)
    uint64_t e_entry;                           // Entry point virtual address
    uint64_t e_phoff;                           // Program header table offset
    uint64_t e_shoff;                           // Section header table offset
    uint64_t e_flags;                           // Processor flags
    uint16_t e_ehsize;                          // ELF header size
    uint16_t e_phentsize;                       // Program header entry size
    uint16_t e_phnum;                           // Number of program headers
    uint16_t e_shentsize;                       // Section header entry size
    uint16_t e_shnum;                           // Number of section headers
    uint16_t e_shstrndx;                        // Section name string table index
} elf64_header_t;

// --- ELF64 program header -------------------------------------------
typedef struct __attribute__((packed))
{
    uint32_t p_type;                            // Segment type (PT_LOAD = 1)
    uint32_t p_flags;                           // Segment flags (PF_R, PF_W, PF_X)
    uint64_t p_offset;                          // Offset in file
    uint64_t p_vaddr;                           // Virtual address in memory
    uint64_t p_addr;                            // Physical address (ignored)
    uint64_t p_filesz;                          // Size in file
    uint64_t p_memsz;                           // Size in memory (>= filesz)
    uint64_t p_align;                           // Alignment
} elf64_phdr_t;

// --- ELF loader interface --------------------------------------------

// Load an ELF binary from disk
// sector = starting disk sector of the ELF binary
// Returns entry point address or 0 on failure
uint64_t elf_load(uint64_t physical_address);

#endif