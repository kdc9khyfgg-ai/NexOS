#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "keyboard.h"   

// Lee un byte de un puerto de entrada/salida.
static inline uint8_t inb(uint16_t puerto) {
    uint8_t valor;

    asm volatile (
        "inb %1, %0"
        : "=a"(valor)
        : "Nd"(puerto)
    );

    return valor;
}

// Intenta leer un scancode del teclado PS/2.
bool keyboard_read_scancode(uint8_t *scancode) {
    if (scancode == NULL) {
        return false;
    }

    // Puerto 0x64: estado del controlador.
    uint8_t estado = inb(0x64);

    // El bit 0 indica que hay un byte disponible.
    if ((estado & 1) == 0) {
        return false;
    }

    // Puerto 0x60: datos del teclado.
    *scancode = inb(0x60);

    return true;
}
