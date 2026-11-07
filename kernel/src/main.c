
#include <stddef.h>

#include <limine.h>

#include "idt.h"
#include "gdt.h"
#include "tss.h"
#include "pmm.h"
#include "pic.h"


// Revision base de Limine.
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] =
LIMINE_BASE_REVISION(6);

// Solicitud de framebuffer.
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// Solicitud del mapa de memoria.
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

// Solicitud del acceso directo a la memoria fisica (HHDM)
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};



// Marcadores de las solicitudes de Limine.
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
LIMINE_REQUESTS_END_MARKER;

// Detiene el kernel de forma segura.
static void hcf(void) {
    __asm__ volatile ("cli");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

// Convierte un canal de color al formato del framebuffer.
static uint32_t fb_channel(
    uint8_t value,
    uint8_t mask_size,
    uint8_t mask_shift
) {
    if (mask_size == 0) {
        return 0;
    }

    uint64_t max = ((uint64_t)1 << mask_size) - 1;

    return (uint32_t)(
        ((uint64_t)value * max / 255) << mask_shift
        );
}

// Construye un color RGB.
static uint32_t fb_pixel(
    struct limine_framebuffer* fb,
    uint8_t red,
    uint8_t green,
    uint8_t blue
) {
    return fb_channel(red, fb->red_mask_size, fb->red_mask_shift)
        | fb_channel(green, fb->green_mask_size, fb->green_mask_shift)
        | fb_channel(blue, fb->blue_mask_size, fb->blue_mask_shift);
}

// Dibuja un píxel.
static void fb_put_pixel(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    uint32_t color
) {
    if (x >= fb->width || y >= fb->height) {
        return;
    }

    volatile uint32_t* pixels = fb->address;
    pixels[y * (fb->pitch / 4) + x] = color;
}

// Fuente de 5 x 7 píxeles para NexOS.
static const uint8_t nexos_font[5][7] = {
    {17, 25, 21, 19, 17, 17, 17}, // N
    {31, 16, 16, 30, 16, 16, 31}, // E
    {17, 17, 10, 4, 10, 17, 17},  // X
    {14, 17, 17, 17, 17, 17, 14}, // O
    {15, 16, 16, 14, 1, 1, 30}    // S
};

// Dibuja el logo NexOS.
static void fb_draw_nexos(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    uint32_t color
) {
    const size_t escala = 8;

    for (size_t letra = 0; letra < 5; letra++) {
        for (size_t fila = 0; fila < 7; fila++) {
            for (size_t columna = 0; columna < 5; columna++) {
                if (nexos_font[letra][fila] &
                    (1u << (4 - columna))) {

                    for (size_t dy = 0; dy < escala; dy++) {
                        for (size_t dx = 0; dx < escala; dx++) {
                            fb_put_pixel(
                                fb,
                                x + (letra * 6 + columna) * escala + dx,
                                y + fila * escala + dy,
                                color
                            );
                        }
                    }
                }
            }
        }
    }
}

// Fuente de letras mayúsculas de 5 x 7 píxeles.
static const uint8_t font[26][7] = {
    {14,17,17,31,17,17,17}, // A
    {30,17,17,30,17,17,30}, // B
    {14,17,16,16,16,17,14}, // C
    {30,17,17,17,17,17,30}, // D
    {31,16,16,30,16,16,31}, // E
    {31,16,16,30,16,16,16}, // F
    {14,17,16,23,17,17,15}, // G
    {17,17,17,31,17,17,17}, // H
    {14,4,4,4,4,4,14},      // I
    {7,2,2,2,2,18,12},      // J
    {17,18,20,24,20,18,17}, // K
    {16,16,16,16,16,16,31}, // L
    {17,27,21,21,17,17,17}, // M
    {17,25,21,19,17,17,17}, // N
    {14,17,17,17,17,17,14}, // O
    {30,17,17,30,16,16,16}, // P
    {14,17,17,17,21,18,13}, // Q
    {30,17,17,30,20,18,17}, // R
    {15,16,16,14,1,1,30},   // S
    {31,4,4,4,4,4,4},       // T
    {17,17,17,17,17,17,14}, // U
    {17,17,17,17,17,10,4},  // V
    {17,17,17,21,21,21,10}, // W
    {17,17,10,4,10,17,17},  // X
    {17,17,10,4,4,4,4},     // Y
    {31,1,2,4,8,16,31}      // Z
};

// Dibuja un carácter ampliado.
static void fb_draw_char(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    char c,
    uint32_t color,
    size_t escala
) {
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, // 0
        {4,12,4,4,4,4,14},      // 1
        {14,17,1,2,4,8,31},     // 2
        {30,1,1,14,1,1,30},     // 3
        {2,6,10,18,31,2,2},     // 4
        {31,16,16,30,1,1,30},   // 5
        {14,16,16,30,17,17,14}, // 6
        {31,1,2,4,8,8,8},       // 7
        {14,17,17,14,17,17,14}, // 8
        {14,17,17,15,1,1,14}    // 9
    };

    // Dibujar dos puntos.
    if (c == ':') {
        const uint8_t colon[7] = { 0, 4, 4, 0, 4, 4, 0 };

        for (size_t fila = 0; fila < 7; fila++) {
            if (colon[fila]) {
                for (size_t dy = 0; dy < escala; dy++) {
                    for (size_t dx = 0; dx < escala; dx++) {
                        fb_put_pixel(
                            fb,
                            x + 2 * escala + dx,
                            y + fila * escala + dy,
                            color
                        );
                    }
                }
            }
        }

        return;
    }

    // Dibujar números.
    if (c >= '0' && c <= '9') {
        const uint8_t* glyph = digits[c - '0'];

        for (size_t fila = 0; fila < 7; fila++) {
            for (size_t columna = 0; columna < 5; columna++) {
                if (glyph[fila] & (1u << (4 - columna))) {
                    for (size_t dy = 0; dy < escala; dy++) {
                        for (size_t dx = 0; dx < escala; dx++) {
                            fb_put_pixel(
                                fb,
                                x + columna * escala + dx,
                                y + fila * escala + dy,
                                color
                            );
                        }
                    }
                }
            }
        }

        return;
    }

    // Dibujar letras.
    if (c < 'A' || c > 'Z') {
        return;
    }

    const uint8_t* glyph = font[c - 'A'];

    for (size_t fila = 0; fila < 7; fila++) {
        for (size_t columna = 0; columna < 5; columna++) {
            if (glyph[fila] & (1u << (4 - columna))) {
                for (size_t dy = 0; dy < escala; dy++) {
                    for (size_t dx = 0; dx < escala; dx++) {
                        fb_put_pixel(
                            fb,
                            x + columna * escala + dx,
                            y + fila * escala + dy,
                            color
                        );
                    }
                }
            }
        }
    }
}

// Lee el fabricante de la CPU mediante CPUID.
static void cpu_get_vendor(char vendor[13]) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
        );

    (void)eax;

    uint32_t registers[3] = { ebx, edx, ecx };

    for (size_t i = 0; i < 12; i++) {
        char c = (char)(
            (registers[i / 4] >> ((i % 4) * 8)) & 0xFF
            );

        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }

        vendor[i] = c;
    }

    vendor[12] = '\0';
}

// Dibuja la información del fabricante debajo del logo.
static void fb_draw_cpu_info(
    struct limine_framebuffer* fb,
    uint32_t color
) {
    char vendor[13];
    cpu_get_vendor(vendor);

    char message[18] = "CPU: ";

    for (size_t i = 0; i < 12; i++) {
        message[5 + i] = vendor[i];
    }

    message[17] = '\0';

    const size_t escala = 2;
    const size_t message_width = 17 * 6 * escala - escala;
    const size_t message_height = 7 * escala;

    size_t x = fb->width > message_width
        ? (fb->width - message_width) / 2
        : 0;

    size_t y = fb->height / 2 + 52;

    if (y + message_height >= fb->height) {
        y = fb->height > message_height
            ? fb->height - message_height - 1
            : 0;
    }

    for (size_t i = 0; i < 17; i++) {
        fb_draw_char(
            fb,
            x + i * 6 * escala,
            y,
            message[i],
            color,
            escala
        );
    }
}

// Calcula la memoria RAM utilizable informada por Limine.
static uint64_t memory_get_usable_mb(void) {
    if (memory_map_request.response == NULL) {
        return 0;
    }

    struct limine_memmap_response* response =
        memory_map_request.response;

    uint64_t total_bytes = 0;

    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        if (entry == NULL) {
            continue;
        }

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (UINT64_MAX - total_bytes < entry->length) {
                total_bytes = UINT64_MAX;
                break;
            }

            total_bytes += entry->length;
        }
    }

    return total_bytes / (1024 * 1024);
}

// Dibuja la cantidad de RAM debajo de la información de CPU.
static void fb_draw_ram_info(
    struct limine_framebuffer* fb,
    uint32_t color
) {
    uint64_t ram_mb = memory_get_usable_mb();

    char message[27] = "RAM: ";
    char number[21];

    size_t digits_count = 0;

    do {
        number[digits_count++] = '0' + (ram_mb % 10);
        ram_mb /= 10;
    } while (ram_mb > 0 && digits_count < sizeof(number));

    size_t length = 5;

    while (digits_count > 0) {
        message[length++] = number[--digits_count];
    }

    const char suffix[] = " MB";

    for (size_t i = 0; i < 3; i++) {
        message[length++] = suffix[i];
    }

    message[length] = '\0';

    const size_t escala = 2;
    const size_t message_width = length * 6 * escala - escala;
    const size_t message_height = 7 * escala;

    size_t x = fb->width > message_width
        ? (fb->width - message_width) / 2
        : 0;

    size_t y = fb->height / 2 + 72;

    if (y + message_height >= fb->height) {
        y = fb->height > message_height
            ? fb->height - message_height - 1
            : 0;
    }

    for (size_t i = 0; i < length; i++) {
        fb_draw_char(
            fb,
            x + i * 6 * escala,
            y,
            message[i],
            color,
            escala
        );
    }
}

// Dibuja el selector del registro TR.
static void fb_draw_tr_info(
    struct limine_framebuffer* fb,
    uint32_t color
) {
    uint16_t tr = gdt_read_tr();

    char message[] = "TR: 00000";

    // Convierte el selector a cinco dígitos decimales.
    for (int i = 8; i >= 4; i--) {
        message[i] = '0' + (tr % 10);
        tr /= 10;
    }

    const size_t escala = 2;
    const size_t length = 9;
    const size_t message_width = length * 6 * escala - escala;
    const size_t message_height = 7 * escala;

    size_t x = fb->width > message_width
        ? (fb->width - message_width) / 2
        : 0;

    size_t y = fb->height / 2 + 92;

    if (y + message_height >= fb->height) {
        y = fb->height > message_height
            ? fb->height - message_height - 1
            : 0;
    }

    // Dibuja cada carácter de la cadena.
    for (size_t i = 0; i < length; i++) {
        fb_draw_char(
            fb,
            x + i * 6 * escala,
            y,
            message[i],
            color,
            escala
        );
    }
}

// Framebuffer que usa el reporte de excepciones.
static struct limine_framebuffer* panic_fb = NULL;

// Nombres de las excepciones 0-31 (sin tildes: la fuente solo tiene A-Z, 0-9 y ':').
static const char* const exception_names[32] = {
    "DIVISION POR CERO",            // 0
    "DEPURACION",                   // 1
    "INTERRUPCION NO ENMASCARABLE", // 2
    "PUNTO DE INTERRUPCION",        // 3
    "DESBORDAMIENTO",               // 4
    "LIMITE EXCEDIDO",              // 5
    "INSTRUCCION INVALIDA",         // 6
    "DISPOSITIVO NO DISPONIBLE",    // 7
    "DOBLE FALLO",                  // 8
    "SEGMENTO DEL COPROCESADOR",    // 9
    "TSS INVALIDA",                 // 10
    "SEGMENTO NO PRESENTE",         // 11
    "FALLO DE PILA",                // 12
    "PROTECCION GENERAL",           // 13
    "FALLO DE PAGINA",              // 14
    "RESERVADA",                    // 15
    "ERROR DE PUNTO FLOTANTE",      // 16
    "ALINEACION",                   // 17
    "MACHINE CHECK",                // 18
    "ERROR SIMD",                   // 19
    "VIRTUALIZACION",               // 20
    "PROTECCION DE CONTROL",        // 21
    "RESERVADA",                    // 22
    "RESERVADA",                    // 23
    "RESERVADA",                    // 24
    "RESERVADA",                    // 25
    "RESERVADA",                    // 26
    "RESERVADA",                    // 27
    "INYECCION DE HYPERVISOR",      // 28
    "COMUNICACION CON VMM",         // 29
    "SEGURIDAD",                    // 30
    "RESERVADA"                     // 31
};

// Convierte un numero a 16 digitos hexadecimales.
static void u64_to_hex(uint64_t value, char out[17]) {
    static const char hex[] = "0123456789ABCDEF";

    for (int i = 15; i >= 0; i--) {
        out[i] = hex[value & 0xF];
        value >>= 4;
    }

    out[16] = '\0';
}

// Dibuja una cadena de texto.
static void fb_draw_text(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    const char* text,
    uint32_t color,
    size_t escala
) {
    for (size_t i = 0; text[i] != '\0'; i++) {
        fb_draw_char(fb, x + i * 6 * escala, y, text[i], color, escala);
    }
}

// Dibuja "ETIQUETA: 0000000000000000".
static void fb_draw_reg(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    const char* label,
    uint64_t value,
    uint32_t color,
    size_t escala
) {
    char hex[17];
    u64_to_hex(value, hex);

    fb_draw_text(fb, x, y, label, color, escala);
    fb_draw_text(fb, x + 8 * 6 * escala, y, hex, color, escala);
}

// Pantalla de error: se llama desde idt_exception_report (idt.c).
void kernel_show_exception(
    const struct idt_exception_frame* frame,
    uint64_t cr2
) {
    struct limine_framebuffer* fb = panic_fb;

    // Si todavia no hay framebuffer, no podemos dibujar nada.
    if (fb == NULL) {
        return;
    }

    uint32_t fondo    = fb_pixel(fb, 120, 0, 0);
    uint32_t blanco   = fb_pixel(fb, 255, 255, 255);
    uint32_t amarillo = fb_pixel(fb, 255, 220, 0);

    // Pinta toda la pantalla de rojo oscuro.
    volatile uint32_t* pixels = fb->address;

    for (size_t y = 0; y < fb->height; y++) {
        for (size_t x = 0; x < fb->width; x++) {
            pixels[y * (fb->pitch / 4) + x] = fondo;
        }
    }

    const size_t escala = 2;
    const size_t margen = 40;
    const size_t linea = 22;

    // Titulo: "EXCEPCION <numero>".
    uint64_t n = frame->exception_number;
    char vec[4];
    size_t len = 0;

    if (n >= 100) vec[len++] = (char)('0' + (n / 100) % 10);
    if (n >= 10)  vec[len++] = (char)('0' + (n / 10) % 10);
    vec[len++] = (char)('0' + n % 10);
    vec[len] = '\0';

    fb_draw_text(fb, margen, 30, "EXCEPCION ", blanco, 3);
    fb_draw_text(fb, margen + 10 * 6 * 3, 30, vec, blanco, 3);

    // Nombre de la excepcion.
    const char* nombre = (n < 32)
        ? exception_names[n]
        : "INTERRUPCION NO MANEJADA";

    fb_draw_text(fb, margen, 70, nombre, amarillo, escala);

    // Registros de control (columna izquierda).
    const struct { const char* nombre; uint64_t valor; } control[] = {
        { "RIP:",    frame->rip },
        { "RSP:",    frame->rsp },
        { "RFLAGS:", frame->rflags },
        { "CS:",     frame->cs },
        { "SS:",     frame->ss },
        { "ERR:",    frame->error_code },
        { "CR2:",    cr2 }
    };

    // CR2 solo tiene sentido en un fallo de pagina (vector 14).
    size_t cantidad_control = (n == 14) ? 7 : 6;
    size_t y0 = 120;

    for (size_t i = 0; i < cantidad_control; i++) {
        fb_draw_reg(fb, margen, y0 + i * linea,
                    control[i].nombre, control[i].valor, blanco, escala);
    }

    // Registros generales (dos columnas, debajo).
    const struct { const char* nombre; uint64_t valor; } generales[] = {
        { "RAX:", frame->rax }, { "RBX:", frame->rbx },
        { "RCX:", frame->rcx }, { "RDX:", frame->rdx },
        { "RSI:", frame->rsi }, { "RDI:", frame->rdi },
        { "RBP:", frame->rbp }, { "R8:",  frame->r8  },
        { "R9:",  frame->r9  }, { "R10:", frame->r10 },
        { "R11:", frame->r11 }, { "R12:", frame->r12 },
        { "R13:", frame->r13 }, { "R14:", frame->r14 },
        { "R15:", frame->r15 }
    };

    size_t y1 = y0 + 8 * linea + 20;
    const size_t ancho_columna = 360;

    for (size_t i = 0; i < 15; i++) {
        size_t columna = i / 8;
        size_t fila = i % 8;

        fb_draw_reg(
            fb,
            margen + columna * ancho_columna,
            y1 + fila * linea,
            generales[i].nombre,
            generales[i].valor,
            blanco,
            escala
        );
    }
}

// Dibuja el fondo degradado.
static void fb_pattern(struct limine_framebuffer* fb) {
    volatile uint32_t* fb_ptr = fb->address;

    for (size_t y = 0; y < fb->height; y++) {
        for (size_t x = 0; x < fb->width; x++) {
            uint8_t red =
                15 + (uint64_t)x * 55 / fb->width;

            uint8_t green =
                8 + (uint64_t)y * 12 / fb->height;

            uint8_t blue =
                45 + (uint64_t)x * 110 / fb->width;

            fb_ptr[y * (fb->pitch / 4) + x] =
                fb_pixel(fb, red, green, blue);
        }
    }
}

// Rellena un rectangulo de un color.
static void fb_fill_rect(
    struct limine_framebuffer* fb,
    size_t x,
    size_t y,
    size_t w,
    size_t h,
    uint32_t color
) {
    for (size_t dy = 0; dy < h; dy++) {
        for (size_t dx = 0; dx < w; dx++) {
            fb_put_pixel(fb, x + dx, y + dy, color);
        }
    }
}

// Muestra cuantas interrupciones de teclado llegaron y el ultimo scancode.
static void fb_draw_keyboard_test(struct limine_framebuffer* fb) {
    const size_t escala = 2;
    const size_t ancho = 8 * 6 * escala + 16 * 6 * escala;
    const size_t alto = 7 * escala;

    size_t x = fb->width > ancho ? (fb->width - ancho) / 2 : 0;
    size_t y = fb->height / 2 + 132;

    uint32_t fondo = fb_pixel(fb, 20, 10, 60);
    uint32_t blanco = fb_pixel(fb, 255, 255, 255);

    fb_fill_rect(fb, x, y, ancho, alto * 2 + 8, fondo);

    fb_draw_reg(fb, x, y, "TECLAS:", idt_irq_count[1], blanco, escala);
    fb_draw_reg(fb, x, y + alto + 8, "SCAN:", idt_last_scancode, blanco, escala);
}

// Punto de entrada del kernel.
void kmain(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        hcf();
    }

    // Comprobamos que Limine haya proporcionado el HHDM.
    if (hhdm_request.response == NULL) {
        hcf();
    }

    // Comprobamos que Limine haya proporcionado el mapa de memoria.
    if (memory_map_request.response == NULL ||
        memory_map_request.response->entries == NULL ||
        memory_map_request.response->entry_count == 0) {
        hcf();
    }

    // Comprobamos el framebuffer.
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer* framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        framebuffer->bpp != 32 ||
        framebuffer->width == 0 ||
        framebuffer->height == 0) {
        hcf();
    }

    // Guardamos el framebuffer para el reporte de excepciones.
    panic_fb = framebuffer;

    // Desactivamos las interrupciones durante la configuración.
    __asm__ volatile ("cli");

    // Inicializamos la TSS.
    tss_init();

    // Cargamos la GDT y registramos la TSS.
    gdt_init();

    // Inicializamos la IDT con los nuevos selectores.
    idt_init();

    // Reubicamos el PIC (vectores 32-47) y dejamos todas las IRQ bloqueadas.
    pic_init();

    // Inicializamos el administrador de memoria física.
    pmm_init(
        memory_map_request.response,
        hhdm_request.response->offset
    );

    // Dibuja el fondo.
    fb_pattern(framebuffer);

    uint32_t blanco = fb_pixel(framebuffer, 255, 255, 255);

    const size_t logo_width = 29 * 8;
    const size_t logo_height = 7 * 8;

    size_t x = framebuffer->width > logo_width
        ? (framebuffer->width - logo_width) / 2
        : 0;

    size_t y = framebuffer->height / 2 >= logo_height / 2
        ? framebuffer->height / 2 - logo_height / 2
        : 0;

    fb_draw_nexos(framebuffer, x, y, blanco);

    // Muestra información de la CPU.
    fb_draw_cpu_info(framebuffer, blanco);

    // Muestra la memoria RAM utilizable.
    fb_draw_ram_info(framebuffer, blanco);

    // Muestra el selector del registro TR.
    fb_draw_tr_info(framebuffer, blanco);

    // PRUEBA TEMPORAL de excepciones: descomentá UNA sola línea para probar.
    // __asm__ volatile ("ud2");                  // vector 6
    // *(volatile uint64_t*)0xDEADBEEF000 = 1;    // vector 14

    // PRUEBA: habilitamos solo la IRQ1 (teclado) y las interrupciones.
    pic_clear_mask(1);
    __asm__ volatile ("sti");

    fb_draw_keyboard_test(framebuffer);

    // Dormimos hasta que llegue una interrupción y redibujamos.
    for (;;) {
        __asm__ volatile ("hlt");
        fb_draw_keyboard_test(framebuffer);
    }
}