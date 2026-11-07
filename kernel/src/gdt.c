#include <stdint.h>
#include <stddef.h>

#include "gdt.h"
#include "tss.h"

struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct {
    uint64_t null;
    uint64_t code;
    uint64_t data;
    struct gdt_tss_entry tss;
} __attribute__((packed)) gdt;

extern void gdt_load(struct gdt_pointer* gdtr);

void gdt_init(void) {
    struct tss* tss = tss_get();

    uintptr_t base = (uintptr_t)tss;
    uint32_t limit = sizeof(struct tss) - 1;

    gdt.null = 0;

    // Segmento de código.
    gdt.code = 0x00AF9A000000FFFF;

    // Segmento de datos.
    gdt.data = 0x00CF92000000FFFF;

    // Entrada TSS de 64 bits.
    gdt.tss.limit_low = limit & 0xFFFF;
    gdt.tss.base_low = base & 0xFFFF;
    gdt.tss.base_middle = (base >> 16) & 0xFF;
    gdt.tss.access = 0x89;
    gdt.tss.limit_high_flags = (limit >> 16) & 0x0F;
    gdt.tss.base_high = (base >> 24) & 0xFF;
    gdt.tss.base_upper = (uint32_t)(base >> 32);
    gdt.tss.reserved = 0;

    struct gdt_pointer gdtr = {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)(uintptr_t)&gdt
    };

    gdt_load(&gdtr);
}

// Lee el selector del registro TR.
uint16_t gdt_read_tr(void) {
    uint16_t selector;

    __asm__ volatile (
        "str %0"
        : "=r"(selector)
        );

    return selector;
}