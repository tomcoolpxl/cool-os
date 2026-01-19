#ifndef KBD_H
#define KBD_H

#include <stdint.h>
#include <stddef.h>

/* PS/2 keyboard ports */
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

/* Status register bits */
#define KBD_STATUS_OUTPUT_FULL  0x01

/* Special key codes */
#define KEY_ESC     27
#define KEY_ENTER   '\n'
#define KEY_TAB     '\t'
#define KEY_BKSP    '\b'

/* Initialize keyboard driver and enable IRQ1 */
void kbd_init(void);

/* Called from IRQ1 handler - process a scancode */
void kbd_handle_irq(void);

/* Translate scancode to ASCII (0 = no character) */
int kbd_translate(uint8_t scancode, int pressed);

/* Get character from input buffer (non-blocking, returns -1 if empty) */
int kbd_getc_nonblock(void);

/* Get character from input buffer (blocking, uses hlt) */
char kbd_getc_blocking(void);

/* Read a line with echo and editing (returns length excluding null) */
size_t kbd_readline(char *dst, size_t max);

#endif
