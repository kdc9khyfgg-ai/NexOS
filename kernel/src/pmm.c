#include "pmm.h"

#define PAGE_SIZE 4096ULL

static uint8_t *pmm_bitmap = NULL;
static uint8_t *pmm_allocated_bitmap = NULL;

static uint64_t pmm_frame_count = 0;
static uint64_t pmm_free_frames = 0;

static uint8_t pmm_initialized = 0;

// Comprueba si una página está ocupada.
static int page_is_used(uint64_t frame) {
    return (pmm_bitmap[frame / 8] &
            (1U << (frame % 8))) != 0;
}

// Comprueba si una página fue asignada por el PMM.
static int page_was_allocated(uint64_t frame) {
    return (pmm_allocated_bitmap[frame / 8] &
            (1U << (frame % 8))) != 0;
}

// Marca una página como ocupada.
static void mark_used(uint64_t frame) {
    if (frame >= pmm_frame_count)
        return;

    if (!page_is_used(frame)) {
        pmm_bitmap[frame / 8] |=
            (uint8_t)(1U << (frame % 8));

        pmm_free_frames--;
    }
}

// Marca una página como libre.
static void mark_free(uint64_t frame) {
    if (frame >= pmm_frame_count)
        return;

    if (page_is_used(frame)) {
        pmm_bitmap[frame / 8] &=
            (uint8_t)~(1U << (frame % 8));

        pmm_free_frames++;
    }
}

// Inicializa el administrador de memoria física.
void pmm_init(
    struct limine_memmap_response *memory_map,
    uint64_t hhdm_offset
) {
    pmm_initialized = 0;
    pmm_bitmap = NULL;
    pmm_allocated_bitmap = NULL;
    pmm_frame_count = 0;
    pmm_free_frames = 0;

    if (memory_map == NULL ||
        memory_map->entries == NULL ||
        memory_map->entry_count == 0) {
        return;
    }

    // Buscamos la dirección física más alta.
    uint64_t highest_address = 0;

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *entry =
            memory_map->entries[i];

        if (entry == NULL || entry->length == 0)
            continue;

        if (entry->base > UINT64_MAX - entry->length)
            continue;

        uint64_t end = entry->base + entry->length;

        if (end > highest_address)
            highest_address = end;
    }

    if (highest_address == 0)
        return;

    // Calculamos la cantidad de páginas físicas.
    uint64_t frame_count = highest_address / PAGE_SIZE;

    if (highest_address % PAGE_SIZE != 0)
        frame_count++;

    // Calculamos el tamaño de un bitmap.
    uint64_t bitmap_bytes = frame_count / 8;

    if (frame_count % 8 != 0)
        bitmap_bytes++;

    // Necesitamos espacio para DOS bitmaps.
    if (bitmap_bytes > UINT64_MAX / 2)
        return;

    uint64_t bitmap_total_bytes = bitmap_bytes * 2;

    if (bitmap_total_bytes > UINT64_MAX - (PAGE_SIZE - 1))
        return;

    uint64_t bitmap_pages =
        (bitmap_total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    if (bitmap_pages == 0 ||
        bitmap_pages > UINT64_MAX / PAGE_SIZE)
        return;

    uint64_t bitmap_storage_size =
        bitmap_pages * PAGE_SIZE;

    // Buscamos espacio usable para guardar los dos bitmaps.
    uint64_t bitmap_physical = UINT64_MAX;

    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *entry =
            memory_map->entries[i];

        if (entry == NULL ||
            entry->type != LIMINE_MEMMAP_USABLE ||
            entry->length == 0) {
            continue;
        }

        if (entry->base > UINT64_MAX - (PAGE_SIZE - 1))
            continue;

        if (entry->base > UINT64_MAX - entry->length)
            continue;

        uint64_t start =
            (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        uint64_t end = entry->base + entry->length;

        if (end < start ||
            end - start < bitmap_storage_size) {
            continue;
        }

        bitmap_physical = start;
        break;
    }

    if (bitmap_physical == UINT64_MAX)
        return;

    // Convertimos la dirección física a una dirección virtual HHDM.
    if (bitmap_physical > UINT64_MAX - hhdm_offset)
        return;

    pmm_bitmap = (uint8_t *)(uintptr_t)
        (bitmap_physical + hhdm_offset);

    // El segundo bitmap comienza después del primero.
    pmm_allocated_bitmap = pmm_bitmap + bitmap_bytes;

    pmm_frame_count = frame_count;

    // Inicialmente todas las páginas están ocupadas.
    for (uint64_t i = 0; i < bitmap_bytes; i++)
        pmm_bitmap[i] = 0xFF;

    // Ninguna página fue asignada todavía por el PMM.
    for (uint64_t i = 0; i < bitmap_bytes; i++)
        pmm_allocated_bitmap[i] = 0;

    // Liberamos únicamente las páginas utilizables.
    for (uint64_t i = 0; i < memory_map->entry_count; i++) {
        struct limine_memmap_entry *entry =
            memory_map->entries[i];

        if (entry == NULL ||
            entry->type != LIMINE_MEMMAP_USABLE ||
            entry->length == 0) {
            continue;
        }

        if (entry->base > UINT64_MAX - (PAGE_SIZE - 1))
            continue;

        if (entry->base > UINT64_MAX - entry->length)
            continue;

        uint64_t start_frame =
            (entry->base + PAGE_SIZE - 1) / PAGE_SIZE;

        uint64_t end_frame =
            (entry->base + entry->length) / PAGE_SIZE;

        if (end_frame > pmm_frame_count)
            end_frame = pmm_frame_count;

        for (uint64_t frame = start_frame;
             frame < end_frame;
             frame++) {
            mark_free(frame);
        }
    }

    // Reservamos la memoria ocupada por los dos bitmaps.
    uint64_t bitmap_first_frame =
        bitmap_physical / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_pages; i++)
        mark_used(bitmap_first_frame + i);

    // La primera página física nunca se entrega.
    mark_used(0);

    pmm_initialized = 1;
}

// Reserva una página física de 4 KiB.
uint64_t pmm_alloc_page(void) {
    if (!pmm_initialized || pmm_free_frames == 0)
        return 0;

    for (uint64_t frame = 1;
         frame < pmm_frame_count;
         frame++) {

        if (!page_is_used(frame)) {
            mark_used(frame);

            // Registramos la asignación.
            pmm_allocated_bitmap[frame / 8] |=
                (uint8_t)(1U << (frame % 8));

            return frame * PAGE_SIZE;
        }
    }

    return 0;
}

// Libera una página física.
void pmm_free_page(uint64_t physical_address) {
    if (!pmm_initialized)
        return;

    if (physical_address == 0 ||
        physical_address % PAGE_SIZE != 0) {
        return;
    }

    uint64_t frame = physical_address / PAGE_SIZE;

    if (frame >= pmm_frame_count)
        return;

    // Solo liberamos páginas asignadas por el PMM.
    if (!page_was_allocated(frame))
        return;

    // Quitamos el registro de asignación.
    pmm_allocated_bitmap[frame / 8] &=
        (uint8_t)~(1U << (frame % 8));

    mark_free(frame);
}

// Devuelve la memoria física libre, en bytes.
uint64_t pmm_get_free_memory(void) {
    if (!pmm_initialized)
        return 0;

    return pmm_free_frames * PAGE_SIZE;
}