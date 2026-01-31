/*
 * stdio.h - Standard I/O functions
 */

#ifndef _STDIO_H
#define _STDIO_H

/* Standard file descriptors */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* Output functions */
int putchar(int c);
int puts(const char *s);
int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* _STDIO_H */
