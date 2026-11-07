#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

#define PMM_PAGE_SIZE 4096

void pmm_init(
    struct limine_memmap_response *memory_map,
    uint64_t hhdm_offser
);

uint64_t pmm_alloc_page(void);

void pmm_free_page(uint64_t physical_address);

uint64_t pmm_get_free_memory(void);

#endif