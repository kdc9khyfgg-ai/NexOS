#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "keyboard.h"   

// Lee un byte de un puerto de entrada/salida
static inline uint8_t inb(uint16_t puerto) {
    uint8_t valor;

    asm volatile (
        "inb %1, %0"
        : "=a"(valor)
        : "Nd"(puerto)
    );

    return valor;
}

// Intenta leer un scancode del teclado PS/2
bool keyboard_read_scancode(uint8_t *scancode) {
    if (scancode == NULL) {
        return false;
    }

    // Puerto 0x64: estado del controlador
    uint8_t estado = inb(0x64);

    // El bit 0 indica que hay un byte disponible
    if ((estado & 1) == 0) {
        return false;
    }

    // Puerto 0x60: datos del teclado
    *scancode = inb(0x60);

    return true;
}


// Traduce scancodes Set 1 a caracteres.
char keyboard_scancode_to_char(uint8_t scancode) {
    static const char tabla[128] = {
        [0x02] = '1',
        [0x03] = '2',
        [0x04] = '3',
        [0x05] = '4',
        [0x06] = '5',
        [0x07] = '6',
        [0x08] = '7',
        [0x09] = '8',
        [0x0A] = '9',
        [0x0B] = '0',

        [0x10] = 'q',
        [0x11] = 'w',
        [0x12] = 'e',
        [0x13] = 'r',
        [0x14] = 't',
        [0x15] = 'y',
        [0x16] = 'u',
        [0x17] = 'i',
        [0x18] = 'o',
        [0x19] = 'p',

        [0x1E] = 'a',
        [0x1F] = 's',
        [0x20] = 'd',
        [0x21] = 'f',
        [0x22] = 'g',
        [0x23] = 'h',
        [0x24] = 'j',
        [0x25] = 'k',
        [0x26] = 'l',

        [0x2C] = 'z',
        [0x2D] = 'x',
        [0x2E] = 'c',
        [0x2F] = 'v',
        [0x30] = 'b',
        [0x31] = 'n',
        [0x32] = 'm',

        [0x39] = ' '
    };

    // Los códigos mayores o iguales a 128 no son válidos aquí.
    if (scancode >= 128) {
        return '\0';
    }

    return tabla[scancode];
}

#define KEYBOARD_BUFFER_SIZE 128

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];

static volatile uint8_t keyboard_head = 0;
static volatile uint8_t keyboard_tail = 0;

// Procesa un scancode y guarda el caracter
void keyboard_process_scancode(uint8_t scancode) {
    char caracter = keyboard_scancode_to_chad(scancode);

    if (caracter == '\0') {
        return:
    }

    uint8_t siguiente =
        (uint8_t)((keyboard_head  1) % KEYBOARD_BUFFER_SIZE);

    // Si esta lleno, descarta el caracter nuevo
    if (siguiente == keyboard_tail) {
        return;
    }

    keyboard_buffer[keyboard_head] = caracter;
    keyboard_head = siguiente;
}

// Obtiene un caracter del bufer
bool keyboard_getchar(char* character) {
    if (character == NULL || keyboard_tail == keyboard_head) {
        return false;
    }

    *character = keyboard_buffer[keyboard_tail];

    keyboard_tail =
        (uint8_t)((keyboard_tail + 1) % KEYBOARD_BUFFER_SIZE);

    return true;
}