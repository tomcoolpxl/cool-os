/*
 * malloc.c - Static bump allocator
 *
 * A simple bump allocator using a fixed 64KB heap.
 * free() is a no-op; memory cannot be reclaimed.
 */

#include <stddef.h>

/* 64 KB static heap */
#define HEAP_SIZE 65536

static char heap[HEAP_SIZE];
static char *heap_ptr;  /* Initialized at runtime to avoid relocation issues */
static int heap_initialized;

void *malloc(size_t size) {
    /* Lazy initialization to handle code relocation */
    if (!heap_initialized) {
        heap_ptr = heap;
        heap_initialized = 1;
    }

    /* Align to 16 bytes */
    size = (size + 15) & ~15UL;

    if (heap_ptr + size > heap + HEAP_SIZE) {
        return NULL;  /* Out of memory */
    }

    void *p = heap_ptr;
    heap_ptr += size;
    return p;
}

void free(void *ptr) {
    /* No-op for bump allocator */
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        /* Zero the memory */
        char *c = p;
        for (size_t i = 0; i < total; i++) {
            c[i] = 0;
        }
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    /* Simple implementation: always allocate new memory */
    /* This leaks the old memory, but works for a bump allocator */
    (void)ptr;
    return malloc(size);
}
