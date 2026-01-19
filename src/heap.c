#include <stdint.h>
#include <stddef.h>
#include "heap.h"
#include "pmm.h"
#include "hhdm.h"
#include "serial.h"
#include "panic.h"

#define HEAP_MAGIC 0xDEADC0DE

typedef struct block {
    uint32_t magic;
    uint32_t free;
    uint64_t size;
    struct block *next;
    struct block *prev;
} block_t;

typedef struct arena {
    struct arena *next;
    uint64_t total_size;
    struct block *first;
} arena_t;

static arena_t *arena_list = NULL;

static void heap_memset(void *dest, uint8_t val, uint64_t count) {
    uint8_t *d = (uint8_t *)dest;
    for (uint64_t i = 0; i < count; i++) {
        d[i] = val;
    }
}

static void print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

static arena_t *heap_expand_size(uint64_t needed_size) {
    /* Calculate how many pages we need */
    uint64_t arena_header_size = ALIGN_UP(sizeof(arena_t), HEAP_ALIGN);
    uint64_t total_needed = arena_header_size + sizeof(block_t) + needed_size;
    uint64_t pages_needed = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed < 1) pages_needed = 1;

    /* Allocate contiguous pages */
    uint64_t first_phys = pmm_alloc_frames_contiguous(pages_needed);
    if (first_phys == 0) {
        return NULL;  /* Out of memory or no contiguous region */
    }

    arena_t *arena = (arena_t *)phys_to_hhdm(first_phys);

    arena->next = NULL;
    arena->total_size = pages_needed * PAGE_SIZE;

    block_t *first = (block_t *)((uint8_t *)arena + arena_header_size);

    first->magic = HEAP_MAGIC;
    first->free = 1;
    first->size = arena->total_size - arena_header_size - sizeof(block_t);
    first->next = NULL;
    first->prev = NULL;

    arena->first = first;

    return arena;
}

static arena_t *heap_expand(void) {
    return heap_expand_size(PAGE_SIZE);  /* Default to one page */
}

void heap_init(void) {
    arena_list = heap_expand();

    serial_puts("HEAP: Initialized with arena at ");
    print_hex((uint64_t)arena_list);
    serial_puts("\n");
}

void *kmalloc(uint64_t size) {
    if (size == 0) {
        return NULL;
    }

    size = ALIGN_UP(size, HEAP_ALIGN);

    arena_t *arena = arena_list;
    while (arena != NULL) {
        block_t *block = arena->first;
        while (block != NULL) {
            ASSERT(block->magic == HEAP_MAGIC);

            if (block->free && block->size >= size) {
                uint64_t remainder = block->size - size;
                uint64_t min_split = sizeof(block_t) + HEAP_ALIGN;

                if (remainder >= min_split) {
                    block_t *new_block = (block_t *)((uint8_t *)block + sizeof(block_t) + size);
                    new_block->magic = HEAP_MAGIC;
                    new_block->free = 1;
                    new_block->size = remainder - sizeof(block_t);
                    new_block->next = block->next;
                    new_block->prev = block;

                    if (block->next != NULL) {
                        block->next->prev = new_block;
                    }
                    block->next = new_block;
                    block->size = size;
                }

                block->free = 0;

                void *payload = (void *)((uint8_t *)block + sizeof(block_t));
                heap_memset(payload, 0xAA, block->size);

                return payload;
            }
            block = block->next;
        }
        arena = arena->next;
    }

    arena_t *new_arena = heap_expand_size(size);
    if (new_arena == NULL) {
        return NULL;  /* Out of memory */
    }

    arena = arena_list;
    while (arena->next != NULL) {
        arena = arena->next;
    }
    arena->next = new_arena;

    return kmalloc(size);
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));

    ASSERT(block->magic == HEAP_MAGIC);
    ASSERT(block->free == 0);

    block->free = 1;

    heap_memset(ptr, 0xDD, block->size);

    if (block->next != NULL && block->next->magic == HEAP_MAGIC && block->next->free) {
        block_t *next = block->next;
        block->size += sizeof(block_t) + next->size;
        block->next = next->next;
        if (next->next != NULL) {
            next->next->prev = block;
        }
    }

    if (block->prev != NULL && block->prev->magic == HEAP_MAGIC && block->prev->free) {
        block_t *prev = block->prev;
        prev->size += sizeof(block_t) + block->size;
        prev->next = block->next;
        if (block->next != NULL) {
            block->next->prev = prev;
        }
    }
}
