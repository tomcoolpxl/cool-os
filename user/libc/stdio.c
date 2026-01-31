/*
 * stdio.c - Standard I/O functions
 */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>

/* From syscall.c */
extern ssize_t write(int fd, const void *buf, size_t len);

/* Output buffer for printf */
#define PRINTF_BUF_SIZE 512

int putchar(int c) {
    char ch = (char)c;
    write(1, &ch, 1);
    return c;
}

int puts(const char *s) {
    size_t len = strlen(s);
    write(1, s, len);
    write(1, "\n", 1);
    return 0;
}

/* Helper: convert unsigned integer to string */
static int uint_to_str(char *buf, unsigned long val, int base, int uppercase) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    int i = 0;

    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (val) {
        tmp[i++] = digits[val % base];
        val /= base;
    }

    /* Reverse into output buffer */
    int len = i;
    while (i > 0) {
        *buf++ = tmp[--i];
    }
    *buf = '\0';
    return len;
}

/* Helper: convert signed integer to string */
static int int_to_str(char *buf, long val) {
    if (val < 0) {
        *buf++ = '-';
        return 1 + uint_to_str(buf, (unsigned long)(-val), 10, 0);
    }
    return uint_to_str(buf, (unsigned long)val, 10, 0);
}

int printf(const char *fmt, ...) {
    va_list ap;
    char buf[PRINTF_BUF_SIZE];
    int pos = 0;
    char numbuf[24];

    va_start(ap, fmt);

    while (*fmt && pos < PRINTF_BUF_SIZE - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && pos < PRINTF_BUF_SIZE - 1) {
                buf[pos++] = *s++;
            }
            break;
        }
        case 'd':
        case 'i': {
            int val = va_arg(ap, int);
            int len = int_to_str(numbuf, val);
            for (int i = 0; i < len && pos < PRINTF_BUF_SIZE - 1; i++) {
                buf[pos++] = numbuf[i];
            }
            break;
        }
        case 'u': {
            unsigned int val = va_arg(ap, unsigned int);
            int len = uint_to_str(numbuf, val, 10, 0);
            for (int i = 0; i < len && pos < PRINTF_BUF_SIZE - 1; i++) {
                buf[pos++] = numbuf[i];
            }
            break;
        }
        case 'x': {
            unsigned int val = va_arg(ap, unsigned int);
            int len = uint_to_str(numbuf, val, 16, 0);
            for (int i = 0; i < len && pos < PRINTF_BUF_SIZE - 1; i++) {
                buf[pos++] = numbuf[i];
            }
            break;
        }
        case 'X': {
            unsigned int val = va_arg(ap, unsigned int);
            int len = uint_to_str(numbuf, val, 16, 1);
            for (int i = 0; i < len && pos < PRINTF_BUF_SIZE - 1; i++) {
                buf[pos++] = numbuf[i];
            }
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(ap, void *);
            buf[pos++] = '0';
            if (pos < PRINTF_BUF_SIZE - 1) buf[pos++] = 'x';
            int len = uint_to_str(numbuf, val, 16, 0);
            for (int i = 0; i < len && pos < PRINTF_BUF_SIZE - 1; i++) {
                buf[pos++] = numbuf[i];
            }
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            buf[pos++] = c;
            break;
        }
        case '%':
            buf[pos++] = '%';
            break;
        case '\0':
            /* Premature end of format string */
            goto done;
        default:
            /* Unknown specifier, just output it */
            buf[pos++] = '%';
            if (pos < PRINTF_BUF_SIZE - 1) {
                buf[pos++] = *fmt;
            }
            break;
        }
        fmt++;
    }

done:
    va_end(ap);

    if (pos > 0) {
        write(1, buf, pos);
    }
    return pos;
}
