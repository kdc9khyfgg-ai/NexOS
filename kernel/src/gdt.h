#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

uint16_t gdt_read_tr(void);

#endif

