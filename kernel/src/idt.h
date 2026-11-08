#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Registros guardados por el handler.
struct idt_exception_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    uint64_t exception_number;
    uint64_t error_code;

    // Los empuja la CPU automaticamente al entrar a la excepcion.
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

// Ultima excepcion registrada.
extern volatile uint64_t idt_last_exception;
extern volatile uint64_t idt_last_error_code;

// Cantidad de veces que llego cada IRQ y ultimo scancode del teclado.
extern volatile uint64_t idt_irq_count[16];
extern volatile uint8_t idt_last_scancode;

void idt_init(void);

void irq_handler(struct idt_exception_frame* frame);

void idt_exception_report(
    struct idt_exception_frame* frame
);

// Dibuja el reporte en pantalla (esta definida en main.c).
void kernel_show_exception(
    const struct idt_exception_frame* frame,
    uint64_t cr2
);

#endif