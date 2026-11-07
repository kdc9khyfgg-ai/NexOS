
#include <stdint.h>

#include "pic.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI 0x20

static uint16_t pic_mask = 0xFFFF;

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    // Desactivamos todas las IRQ mientras configuramos.
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    // Comenzamos la inicialización.
    outb(PIC1_COMMAND, 0x11);
    io_wait();

    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // Reubicamos las interrupciones:
    // PIC maestro: vectores 32-39.
    // PIC esclavo: vectores 40-47.
    outb(PIC1_DATA, 0x20);
    io_wait();

    outb(PIC2_DATA, 0x28);
    io_wait();

    // Configuramos la conexión entre ambos PIC.
    outb(PIC1_DATA, 0x04);
    io_wait();

    outb(PIC2_DATA, 0x02);
    io_wait();

    // Modo 8086.
    outb(PIC1_DATA, 0x01);
    io_wait();

    outb(PIC2_DATA, 0x01);
    io_wait();

    // Dejamos todas las IRQ bloqueadas.
    pic_mask = 0xFFFF;
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    // Si viene del PIC esclavo, avisamos primero a ese PIC.
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    // Después avisamos al PIC maestro.
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    if (irq >= 16) {
        return;
    }

    pic_mask |= (uint16_t)(1u << irq);

    outb(PIC1_DATA, (uint8_t)(pic_mask & 0xFF));
    outb(PIC2_DATA, (uint8_t)(pic_mask >> 8));
}

void pic_clear_mask(uint8_t irq) {
    if (irq >= 16) {
        return;
    }

    pic_mask &= (uint16_t)~(1u << irq);

    outb(PIC1_DATA, (uint8_t)(pic_mask & 0xFF));
    outb(PIC2_DATA, (uint8_t)(pic_mask >> 8));
}
