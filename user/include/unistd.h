/*
 * unistd.h - Standard symbolic constants and types
 *
 * POSIX-like system call wrappers.
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>

/* Standard file descriptors */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* System calls */
ssize_t write(int fd, const void *buf, size_t len);
void yield(void);

/* Process management (Proto 15) */
int wait(int *status);
uint32_t getpid(void);
uint32_t getppid(void);

/* Note: exit() is in stdlib.h as per standard C */

#endif /* _UNISTD_H */
