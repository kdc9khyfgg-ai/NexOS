#include <stdint.h>
#include <stddef.h>

#include "idt.h"
#include "pic.h"

static struct idt_entry idt_table[IDT_ENTRIES];

extern void idt_default_handler(void);

#define DECLARE_ISR(n) extern void isr##n(void)

DECLARE_ISR(0);
DECLARE_ISR(1);
DECLARE_ISR(2);
DECLARE_ISR(3);
DECLARE_ISR(4);
DECLARE_ISR(5);
DECLARE_ISR(6);
DECLARE_ISR(7);
DECLARE_ISR(8);
DECLARE_ISR(9);
DECLARE_ISR(10);
DECLARE_ISR(11);
DECLARE_ISR(12);
DECLARE_ISR(13);
DECLARE_ISR(14);
DECLARE_ISR(15);
DECLARE_ISR(16);
DECLARE_ISR(17);
DECLARE_ISR(18);
DECLARE_ISR(19);
DECLARE_ISR(20);
DECLARE_ISR(21);
DECLARE_ISR(22);
DECLARE_ISR(23);
DECLARE_ISR(24);
DECLARE_ISR(25);
DECLARE_ISR(26);
DECLARE_ISR(27);
DECLARE_ISR(28);
DECLARE_ISR(29);
DECLARE_ISR(30);
DECLARE_ISR(31);

#undef DECLARE_ISR

#define DECLARE_IRQ(n) extern void irq##n(void)

DECLARE_IRQ(0);
DECLARE_IRQ(1);
DECLARE_IRQ(2);
DECLARE_IRQ(3);
DECLARE_IRQ(4);
DECLARE_IRQ(5);
DECLARE_IRQ(6);
DECLARE_IRQ(7);
DECLARE_IRQ(8);
DECLARE_IRQ(9);
DECLARE_IRQ(10);
DECLARE_IRQ(11);
DECLARE_IRQ(12);
DECLARE_IRQ(13);
DECLARE_IRQ(14);
DECLARE_IRQ(15);

#undef DECLARE_IRQ

static void (* const irq_stubs[16])(void) = {
    irq0, irq1, irq2, irq3,
    irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11,
    irq12, irq13, irq14, irq15
};

volatile uint64_t idt_irq_count[16];
volatile uint8_t idt_last_scancode = 0;

static void (* const exception_handlers[32])(void) = {
    isr0, isr1, isr2, isr3,
    isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11,
    isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19,
    isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27,
    isr28, isr29, isr30, isr31
};

volatile uint64_t idt_last_exception = 0;
volatile uint64_t idt_last_error_code = 0;

static void idt_set_gate(
    uint8_t vector,
    uintptr_t address,
    uint16_t selector,
    uint8_t ist_index
) {
    idt_table[vector].offset_low = (uint16_t)(address & 0xFFFF);
    idt_table[vector].selector = selector;

    // Seleccionamos la pila IST correspondiente
    idt_table[vector].ist = ist_index & 0x07;

    idt_table[vector].attributes = 0x8E;
    idt_table[vector].offset_middle = (uint16_t)((address >> 16) & 0xFFFF);
    idt_table[vector].offset_high = (uint32_t)(address >> 32);
    idt_table[vector].reserved = 0;
}

void idt_init(void) {

    // Evitamos interrupciones mientras configuramos la IDT

    __asm volatile ("cli");

    uint16_t code_selector;


    __asm__ volatile (
        "mov %%cs, %0"
        : "=r"(code_selector)
        );

    // Inicializamos las 256 entradas.
    uintptr_t default_address =
        (uintptr_t)idt_default_handler;

    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i,default_address, code_selector, 0);
    }

    // Instalamos los 32 handlers de excepciones.
    for (uint8_t i = 0; i < 32; i++) {
        idt_set_gate(
            i,
            (uintptr_t)exception_handlers[i],
            code_selector,
            (i == 8) ? 1 : 0
        );
    }

    // Instalamos los 16 handlers de IRQ (vectores 32-47).
    for (uint8_t i = 0; i < 16; i++) {
        idt_set_gate(
            (uint8_t)(32 + i),
            (uintptr_t)irq_stubs[i],
            code_selector,
            0
        );
    }

    struct idt_pointer idtr;

    idtr.limit = (uint16_t)(sizeof(idt_table) - 1);
    idtr.base = (uint64_t)(uintptr_t)idt_table;

    __asm__ volatile (
        "lidt %0"
        :
    : "m"(idtr)
        : "memory"
        );
}

// Evita que una excepcion dentro del reporte nos mande a un bucle infinito.
static volatile int idt_reportando = 0;

void idt_exception_report(
    struct idt_exception_frame* frame
) {
    __asm__ volatile ("cli");

    // Si ya estabamos reportando (o el frame es invalido), solo nos detenemos.
    if (idt_reportando || frame == NULL) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    idt_reportando = 1;

    idt_last_exception = frame->exception_number;
    idt_last_error_code = frame->error_code;

    // CR2 guarda la direccion que causo un fallo de pagina.
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    kernel_show_exception(frame, cr2);

    // El kernel se detiene despues de mostrar el error.
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

// Lee un byte de un puerto de entrada/salida.
static uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

// Handler general de interrupciones externas (IRQ).
void irq_handler(struct idt_exception_frame* frame) {
    uint8_t irq = (uint8_t)(frame->exception_number - 32);

    if (irq < 16) {
        idt_irq_count[irq]++;
    }

    // Teclado: hay que leer el puerto 0x60 o el controlador no envia mas.
    if (irq == 1) {
        idt_last_scancode = inb(0x60);
    }

    // Avisamos al PIC que terminamos, o no llegan mas interrupciones.
    pic_send_eoi(irq);
}