#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "driver/serial.h"

// These bits control how the MMU treats a specific memory region.
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_PWT (1ULL << 3)  
#define PAGE_PCD (1ULL << 4)
#define PAGE_HUGE (1ULL << 7)

/**
 * We need a way to create new page tables on the fly. Since we don't 
 * have a complex heap yet, we bump a pointer through a known free 
 * memory region (starting at 1MB). 
 */
static uint64_t next_free_page = 0x100000;

void *memset(void *dest, int value, size_t count)
{
    uint8_t *ptr = (uint8_t *)dest;
    uint8_t val = (uint8_t)value;

    for (size_t i = 0; i < count; i++)
    {
        ptr[i] = val;
    }

    return dest;
}  

/**
 * @brief Allocates and zeroes a 4KB block of memory for a new page table.
 * Page tables must be 4KB aligned. Stale data in 
 * a new table could be interpreted by the CPU as valid present mappings.
 */
static uint64_t alloc_page_table(void) 
{
    // Check if the current pointer is 4KB aligned (lower 12 bits must be 0)
    if (next_free_page & 0xFFF) 
    {
        serial_print("Page table allocation is not 4KB aligned\n");
        hcf(); 
    }

    uint64_t addr = next_free_page;
    next_free_page += 0x1000;

    uint64_t* pt = (uint64_t*)(uintptr_t)addr;
    memset((void*)pt, 0, 4096);

    return addr;
}

static inline void invlpg(uintptr_t addr) 
{
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

/**
 * @brief Walks the 4-level page hierarchy to map a virtual address to a physical one.
 * In 64-bit mode, the CPU doesn't know where a physical address is until 
 * we define it in the tables. This function goes through the levels:
 * PML4 -> PDPT -> PD -> PT.
 */
void map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags) 
{
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);

    // Calculate indices based on virtual address
    uint64_t pml4_i = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdpt_i = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_i = (virtual_addr >> 21) & 0x1FF;
    uint64_t pt_i = (virtual_addr >> 12) & 0x1FF;

    if (!(pml4[pml4_i] & PAGE_PRESENT)) 
    {
        pml4[pml4_i] = alloc_page_table() | PAGE_PRESENT | PAGE_WRITE;
    }
    
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[pml4_i] & ~0xFFFULL);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) 
    {
        pdpt[pdpt_i] = alloc_page_table() | PAGE_PRESENT | PAGE_WRITE;
    }
    
    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_i] & ~0xFFFULL);

    if (!(pd[pd_i] & PAGE_PRESENT)) 
    {
        pd[pd_i] = alloc_page_table() | PAGE_PRESENT | PAGE_WRITE;
    }
    
    uint64_t* pt = (uint64_t*)(uintptr_t)(pd[pd_i] & ~0xFFFULL);

    // Virtual address index points to physical address
    pt[pt_i] = (physical_addr & ~0xFFFULL) | flags;

    invlpg(virtual_addr);
}

/**
 * @brief Maps a 2MB huge page by setting the PAGE_HUGE bit in the page directory.
 * This skips the 4th level (page table) entirely.
 */
void map_huge_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags) 
{
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);

    // Calculate indices
    uint64_t pml4_i = (virtual_addr >> 39) & 0x1FF;
    uint64_t pdpt_i = (virtual_addr >> 30) & 0x1FF;
    uint64_t pd_i = (virtual_addr >> 21) & 0x1FF;

    // PML4 -> PDPT
    if (!(pml4[pml4_i] & PAGE_PRESENT)) 
    {
        pml4[pml4_i] = alloc_page_table() | PAGE_PRESENT | PAGE_WRITE;
    }
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[pml4_i] & ~0xFFFULL);

    // PDPT -> PD
    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) 
    {
        pdpt[pdpt_i] = alloc_page_table() | PAGE_PRESENT | PAGE_WRITE;
    }
    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_i] & ~0xFFFULL);

    // For a huge page, the PD entry points to physical address, not a PT.
    // Address must be 2MB aligned (masking lower 21 bits).
    pd[pd_i] = (physical_addr & ~0x1FFFFFULL) | flags | PAGE_HUGE;

    invlpg(virtual_addr);
}

/**
 * @brief Maps a range of memory for Memory-Mapped I/O (MMIO).
 * Hardware like AHCI (SATA) is controlled via memory addresses. 
 * We must map these with PAGE_PCD (cache disable).
 * If we allowed the CPU to cache MMIO registers, we might read 
 * a stale status bit from the CPU cache instead of the actual hardware.
 */
void map_mmio_region(uint64_t physical_addr, uint64_t size) 
{
    uint64_t addr = physical_addr & ~0xFFFULL;
    uint64_t end = (physical_addr + size + 0xFFF) & ~0xFFFULL;
    uint64_t flags = PAGE_PRESENT | PAGE_WRITE | PAGE_PCD | PAGE_PWT;

    while (addr < end) 
    {
        /**
         * Mapping a 1GB AHCI BAR with 4KB pages requires 
         * 262,144 page table entries. Using 2MB pages reduces 
         * this to only 512 entries in the page 
         * directory, improving TLB efficiency.
         */
        if ((addr % 0x200000 == 0) && (end - addr >= 0x200000)) 
        {
            map_huge_page(addr, addr, flags | PAGE_HUGE);
            addr += 0x200000;
        } 
        else 
        {
            map_page(addr, addr, flags);
            addr += 0x1000;
        }
    }

    serial_print("MMIO region mapped successfully\n");
}