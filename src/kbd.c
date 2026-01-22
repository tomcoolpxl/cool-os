#include "kbd.h"
#include "ports.h"
#include "pic.h"
#include "serial.h"
#include "console.h"
#include "framebuffer.h"

/* Modifier key states */
static int shift_left;
static int shift_right;
static int caps_lock;
static int ctrl_held;
static int extended_scancode;  /* E0 prefix seen */

/* Ring buffer for input */
#define KBD_BUFFER_SIZE 256
static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_head;  /* Write position (IRQ context) */
static volatile uint32_t kbd_tail;  /* Read position (consumer) */

/*
 * Scancode Set 1 - Normal (unshifted) key mappings
 * Index is scancode, value is ASCII character (0 = no char)
 */
static const char scancode_normal[128] = {
    /* 0x00 */ 0,    27,   '1',  '2',  '3',  '4',  '5',  '6',
    /* 0x08 */ '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    /* 0x10 */ 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
    /* 0x18 */ 'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
    /* 0x20 */ 'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
    /* 0x28 */ '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    /* 0x30 */ 'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
    /* 0x38 */ 0,    ' ',  0,    0,    0,    0,    0,    0,
    /* 0x40 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x48 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x50 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x58 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x60 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x68 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x70 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x78 */ 0,    0,    0,    0,    0,    0,    0,    0,
};

/*
 * Scancode Set 1 - Shifted key mappings
 */
static const char scancode_shifted[128] = {
    /* 0x00 */ 0,    27,   '!',  '@',  '#',  '$',  '%',  '^',
    /* 0x08 */ '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
    /* 0x10 */ 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
    /* 0x18 */ 'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
    /* 0x20 */ 'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
    /* 0x28 */ '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    /* 0x30 */ 'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
    /* 0x38 */ 0,    ' ',  0,    0,    0,    0,    0,    0,
    /* 0x40 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x48 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x50 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x58 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x60 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x68 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x70 */ 0,    0,    0,    0,    0,    0,    0,    0,
    /* 0x78 */ 0,    0,    0,    0,    0,    0,    0,    0,
};

/* Scancode constants for modifier keys */
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CTRL     0x1D
#define SC_CAPS     0x3A
#define SC_ALT      0x38

void kbd_init(void) {
    /* Flush any pending data from keyboard controller */
    while (inb(KBD_STATUS_PORT) & KBD_STATUS_OUTPUT_FULL) {
        inb(KBD_DATA_PORT);
    }

    /* Initialize state */
    shift_left = 0;
    shift_right = 0;
    caps_lock = 0;
    ctrl_held = 0;
    extended_scancode = 0;
    kbd_head = 0;
    kbd_tail = 0;

    /* Enable IRQ1 (keyboard) on PIC */
    pic_clear_mask(1);

    serial_puts("KBD: init\n");
    serial_puts("KBD: IRQ1 enabled\n");
}

int kbd_translate(uint8_t scancode, int pressed) {
    /* Only handle key press events */
    if (!pressed) {
        return 0;
    }

    /* Ignore scancodes outside our table */
    if (scancode >= 128) {
        return 0;
    }

    /* Get base character from appropriate table */
    int shifted = shift_left || shift_right;
    char c = shifted ? scancode_shifted[scancode] : scancode_normal[scancode];

    /* Apply caps lock to letters (XOR with shift for proper behavior) */
    if (caps_lock && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';  /* Make uppercase */
    } else if (caps_lock && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';  /* Make lowercase (shift+caps = lower) */
    }

    return c;
}

void kbd_process_scancode(uint8_t key, int pressed) {
    /* Handle modifier keys */
    switch (key) {
        case SC_LSHIFT:
            shift_left = pressed;
            return;
        case SC_RSHIFT:
            shift_right = pressed;
            return;
        case SC_CTRL:
            ctrl_held = pressed;
            return;
        case SC_CAPS:
            if (pressed) {
                caps_lock = !caps_lock;  /* Toggle on press only */
            }
            return;
        case SC_ALT:
            /* Track but don't use for now */
            return;
    }

    /* Translate scancode to ASCII */
    int c = kbd_translate(key, pressed);

    /* If we got a character, add to ring buffer */
    if (c != 0) {
        uint32_t next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
        if (next_head != kbd_tail) {
            kbd_buffer[kbd_head] = (char)c;
            kbd_head = next_head;
        }
    }
}

void kbd_handle_irq(void) {
    /* Read scancode from data port */
    uint8_t scancode = inb(KBD_DATA_PORT);

    /* Handle extended scancode prefix (0xE0) */
    if (scancode == 0xE0) {
        extended_scancode = 1;
        return;
    }

    /* For extended scancodes, just clear flag and ignore for now */
    if (extended_scancode) {
        extended_scancode = 0;
        return;
    }

    /* Determine if this is a press or release event */
    int pressed = !(scancode & 0x80);
    uint8_t key = scancode & 0x7F;

    kbd_process_scancode(key, pressed);
}

int kbd_getc_nonblock(void) {
    int c;

    /* Disable interrupts for atomic buffer access */
    asm volatile("cli");

    if (kbd_head == kbd_tail) {
        /* Buffer empty */
        asm volatile("sti");
        return -1;
    }

    c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;

    asm volatile("sti");
    return c;
}

char kbd_getc_blocking(void) {
    int c;

    for (;;) {
        /* Enable interrupts and wait */
        asm volatile("sti");
        asm volatile("hlt");

        /* Check buffer (with interrupts disabled) */
        asm volatile("cli");
        if (kbd_head != kbd_tail) {
            c = kbd_buffer[kbd_tail];
            kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
            asm volatile("sti");
            return (char)c;
        }
    }
}

size_t kbd_readline(char *dst, size_t max) {
    size_t pos = 0;

    if (max == 0) {
        return 0;
    }

    /* Leave room for null terminator */
    max--;

    while (pos < max) {
        char c = kbd_getc_blocking();

        if (c == '\n') {
            /* Enter pressed - finish line */
            console_putc('\n');
            fb_present();
            break;
        } else if (c == '\b') {
            /* Backspace - remove last character if any */
            if (pos > 0) {
                pos--;
                console_erase_char();
            }
        } else if (c >= 32 && c < 127) {
            /* Printable character */
            dst[pos++] = c;
            console_putc(c);
            fb_present();
        }
        /* Ignore other control characters */
    }

    dst[pos] = '\0';
    return pos;
}
