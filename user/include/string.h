/*
 * string.h - String and memory functions
 */

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* String functions */
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

/* Memory functions */
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int val, size_t n);

#endif /* _STRING_H */
