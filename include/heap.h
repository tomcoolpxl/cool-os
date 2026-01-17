#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

#define HEAP_ALIGN 16
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

void heap_init(void);
void *kmalloc(uint64_t size);
void kfree(void *ptr);

#endif
