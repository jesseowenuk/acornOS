#ifndef KERNEL_PAGING_H
#define KERNEL_PAGING_H

#include <stdint.h>

#include <kernel/interrupts.h>

// --- Forward declarations ------------------------------------------------
struct page_directory;
typedef struct page_directory page_directory_t;

// --- Page Flags -----------------------------------------------------------
#define PAGE_PRESENT            (1UL << 0)      // Page is present
#define PAGE_WRITABLE           (1UL << 1)      // Page is writable
#define PAGE_USER               (1UL << 2)      // User space accessible
#define PAGE_HUGE               (1UL << 7)      // 2MB (PD) or 1GB (PDPT) page
#define PAGE_NO_EXECUTE         (1UL << 63)     // Disable execution

// --- Interface -----------------------------------------------------------

// Initialise paging subsystem
void paging_init();

// Map a single 4KB page: virtual -> physical with given flags
void map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);

// Map a page in a specific address space
void map_page_in(page_directory_t* pml4, uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);

// Unmap a page
void unmap_page(uint64_t virtual_addr);

// Walk page tables to find physical address for virtual address
// Returns 0 if not mapped
uint64_t get_physical(uint64_t virtual_addr);

// Create a new address space with kernel mappings shared
page_directory_t* paging_clone_directory();

// Switch to a different address space
void paging_switch_directory(page_directory_t* pml4);

// Deep copy an address space (for fork())
page_directory_t* paging_deep_copy_directory(page_directory_t* src);

// Page fault handler - called from IDT
void page_fault_handler(registers_t* regs);

uint64_t physical_to_virtual(uint64_t physical);

uint64_t virtual_to_physical(uint64_t virtual);

#endif