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
    /* Check if controller exists (if read returns 0xFF, likely no device) */
    if (inb(KBD_STATUS_PORT) == 0xFF) {
        serial_puts("KBD: Controller not found (0xFF)\n");
        return;
    }

    /* Flush any pending data from keyboard controller */
    int timeout = 10000;
    while ((inb(KBD_STATUS_PORT) & KBD_STATUS_OUTPUT_FULL) && timeout-- > 0) {
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

    serial_puts("KBD: Scancode: ");
    serial_print_hex(scancode);
    serial_puts("\n");

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

#ifdef REGTEST_BUILD
/*
 * Character to Scancode Set 1 mapping for test injection.
 * Index is ASCII character, value is make scancode.
 */
static const uint8_t char_to_scancode[128] = {
    /* 0x00-0x0F: Control characters */
    ['\b'] = 0x0E,  /* Backspace */
    ['\t'] = 0x0F,  /* Tab */
    ['\n'] = 0x1C,  /* Enter */

    /* 0x20-0x2F: Space and punctuation */
    [' ']  = 0x39,
    ['!']  = 0x02,  /* Shift+1 - just use 1 for simplicity */
    ['\''] = 0x28,
    [',']  = 0x33,
    ['-']  = 0x0C,
    ['.']  = 0x34,
    ['/']  = 0x35,

    /* 0x30-0x39: Digits */
    ['0']  = 0x0B,
    ['1']  = 0x02,
    ['2']  = 0x03,
    ['3']  = 0x04,
    ['4']  = 0x05,
    ['5']  = 0x06,
    ['6']  = 0x07,
    ['7']  = 0x08,
    ['8']  = 0x09,
    ['9']  = 0x0A,

    /* 0x3A-0x40: More punctuation */
    [';']  = 0x27,
    ['=']  = 0x0D,
    ['[']  = 0x1A,
    ['\\'] = 0x2B,
    [']']  = 0x1B,
    ['`']  = 0x29,

    /* 0x41-0x5A: Uppercase letters (same scancode as lowercase) */
    ['A']  = 0x1E, ['B']  = 0x30, ['C']  = 0x2E, ['D']  = 0x20,
    ['E']  = 0x12, ['F']  = 0x21, ['G']  = 0x22, ['H']  = 0x23,
    ['I']  = 0x17, ['J']  = 0x24, ['K']  = 0x25, ['L']  = 0x26,
    ['M']  = 0x32, ['N']  = 0x31, ['O']  = 0x18, ['P']  = 0x19,
    ['Q']  = 0x10, ['R']  = 0x13, ['S']  = 0x1F, ['T']  = 0x14,
    ['U']  = 0x16, ['V']  = 0x2F, ['W']  = 0x11, ['X']  = 0x2D,
    ['Y']  = 0x15, ['Z']  = 0x2C,

    /* 0x61-0x7A: Lowercase letters */
    ['a']  = 0x1E, ['b']  = 0x30, ['c']  = 0x2E, ['d']  = 0x20,
    ['e']  = 0x12, ['f']  = 0x21, ['g']  = 0x22, ['h']  = 0x23,
    ['i']  = 0x17, ['j']  = 0x24, ['k']  = 0x25, ['l']  = 0x26,
    ['m']  = 0x32, ['n']  = 0x31, ['o']  = 0x18, ['p']  = 0x19,
    ['q']  = 0x10, ['r']  = 0x13, ['s']  = 0x1F, ['t']  = 0x14,
    ['u']  = 0x16, ['v']  = 0x2F, ['w']  = 0x11, ['x']  = 0x2D,
    ['y']  = 0x15, ['z']  = 0x2C,
};

void kbd_inject_string(const char *s) {
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 128) {
            uint8_t scancode = char_to_scancode[c];
            if (scancode != 0) {
                /* Simulate key press (no release needed for buffer injection) */
                kbd_process_scancode(scancode, 1);
            }
        }
        s++;
    }
}

void kbd_reset_state(void) {
    /* Disable interrupts for atomic state reset */
    asm volatile("cli");

    /* Reset modifier state */
    shift_left = 0;
    shift_right = 0;
    caps_lock = 0;
    ctrl_held = 0;
    extended_scancode = 0;

    /* Clear ring buffer */
    kbd_head = 0;
    kbd_tail = 0;

    asm volatile("sti");
}
#endif /* REGTEST_BUILD */
