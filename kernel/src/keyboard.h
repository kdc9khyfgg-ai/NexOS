
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Intenta leer un código del teclado.
// Devuelve true si hay un código disponible.
bool keyboard_read_scancode(uint8_t *scancode);

#endif
