
#include <stdint.h>
#include <stddef.h>

#include "tss.h"

static struct tss kernel_tss;

#define IST_STACK_SIZE 16384

static uint8_t double_fault_stack[IST_STACK_SIZE]
    __attribute__((aligned(16)));

void tss_init(void) {
    kernel_tss = (struct tss){0};

    kernel_tss.ist1 =
        (uint64_t)(uintptr_t)(
            double_fault_stack + IST_STACK_SIZE
        );

    kernel_tss.iomap_base = sizeof(struct tss);
}

struct tss *tss_get(void) {
    return &kernel_tss;
}

