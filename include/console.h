#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_clear(void);

#endif
