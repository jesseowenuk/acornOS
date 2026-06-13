#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Each page is 4KB - the standard x86 page size
// Everything in the PMM is aligned to this boundary
#define PAGE_SIZE 4096

// The bitmap lives at a fixed address just above our heap
// Each bit represents one 4KB page of physcial memory
// 1MB mark - heap must start above this!
#define PMM_BITMAP_ADDRESS 0x100000  // 1MB mark

// Structure of a single E820 memory map entry
// Must match exactly what the BIOS wrote during boot
typedef struct
{
    uint64_t base;          // Starting physical address of this region
    uint64_t length;        // Length of this region in bytes
    uint64_t type;          // 1=usable, 2=reserved, 3=ACPI, 4=ACPI NVS, 5=bad
    uint64_t acpi;          // ACPI extended attributes - we wrote 1 here in boot.asm
} __attribute__((packed)) e820_entry_t;

// E820 memory type constants - makes code more readable that raw numbers
#define E820_USABLE     1   // Free RAM we can use
#define E820_RESERVED   2   // Hardware or firmware - do not touch
#define E820_ACPI       3   // ACPI tables - can reclaim after parsing
#define E820_ACPI_NVS   4   // ACPI non-volatile - firmware needs this
#define E820_BAD        5   // Faulty RAM - never use

// Reads E820 map and builds the bitmap
void pmm_init(uint64_t mem_map_addr, uint64_t mem_map_count);

// Allocate one free page - returns physical address
void* pmm_alloc();

// Free a previously allocated page
void pmm_free(void* page);

// How many free pages remain
uint64_t pmm_free_pages();

// How many pages are in use?
uint64_t pmm_used_pages();

// Print memory stats to VGA
void pmm_print_stats();

#endif