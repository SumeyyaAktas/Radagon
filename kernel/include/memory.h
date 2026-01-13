#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdint.h>

void *memset(void *dest, int value, size_t count);
void map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
void map_huge_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
void map_mmio_region(uint64_t physical_addr, uint64_t size);

#endif