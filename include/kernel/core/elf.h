#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#include <kernel/paging.h>
#include <kernel/processes/process.h>

#include <stdint.h>

// TEMPORARY: hello.elf is loaded by stage 2 to this fixed physical address
// Will be removed once we have a persistent file system (barkFS) and 
// load ELF binaries from disk via the VFS instead
#define HELLO_ELF_PHYSICAL_ADDRESS 0x300000

// Must match the sector count in boot/stage2.asm's load_elf (and
// its matching copy-to-0x300000 step) - the whole boot-time buffer,
// including trailing zero padding past the real ELF content. Used to
// also write hello.elf into shadowFS at boot, so it's runnable (and
// re-runnable, with different argv) via the shell's 'run' command -
// not just the fixed boot-time slot above
#define HELLO_ELF_SIZE (128 * 512)

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

// Get the elf entry point
// data = kernel virtual pointer to the start of the ELF image in memory
uint64_t elf_get_entry(uint8_t* data);

// Load an ELF64 image already resident in memory into a process's
// address space. data = kernel virtual pointer to the start of the
// ELF image (already-mapped physical memory, or a heap buffer).
// argv = NULL-terminated array of argument strings (argv[0] is
// conventionally the program name - the caller's job to set that, not
// this function's); or NULL for no arguments. Strings are copied onto
// the new process's stack before this returns, so the caller's argv
// array and strings only need to stay valid until then.
// Returns entry point address or 0 on failure
uint64_t elf_load(uint8_t* data, process_t* process, char** argv);

// Load an ELF64 from a VFS path into a process's address space.
// Reads the whole file into a temporary heap buffer, then calls elf_load().
// Returns entry point address or 0 on failure (file missing, bad ELF, OOM)
uint64_t elf_load_from_path(const char* path, process_t* process, char** argv);

#endif