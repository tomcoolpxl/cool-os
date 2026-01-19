```md
# Prototype 12: Keyboard Input (PS/2), Keymap, and Input Buffer

## Purpose

Add interactive input by implementing a PS/2 keyboard driver (i8042 controller), IRQ1 handling, scancode decoding, and a line-oriented input buffer. This enables:

- Kernel-side interactive commands (first minimal shell later)
- User program input via syscalls (later)
- Basic UI control for graphics demos

This prototype targets QEMU and typical PC hardware that still exposes a PS/2-compatible keyboard interface (including via USB keyboard emulation in firmware).

## Scope

In scope:
- PIC unmasking for IRQ1
- IRQ1 handler and EOI
- Reading scancodes from port 0x60
- Scancode Set 1 decoding (press/release)
- Shift and Caps Lock state
- ASCII translation for common keys
- Kernel input ring buffer
- Blocking line read using hlt (interrupt-driven)
- Console echo and basic editing (backspace)

Out of scope:
- USB HID stack
- Key repeat rate programming
- International layouts
- Compose keys, dead keys
- Mouse input
- IME
- Full TTY job control

## Dependencies

Required components:
- Working IDT with IRQ return path (Prototype 4)
- PIC driver (remap, masks, EOI)
- Timer and hlt-based sleep/wait (Prototype 5)
- Backbuffer framebuffer + console (Prototype 11)
- ports.h inb/outb helpers
- panic() and ASSERT

## Hardware and IRQ Model

- PS/2 keyboard data port: 0x60
- PS/2 status port: 0x64
- Keyboard IRQ: IRQ1 -> vector 0x21 after PIC remap to 0x20

Kernel must:
- Unmask IRQ1 on PIC master
- Register an IRQ stub/handler for vector 0x21
- Send EOI after processing scancode

## Functional Requirements

### 1. IRQ1 Enablement

On init:
- pic_clear_mask(1) to unmask IRQ1
- Ensure your vector 0x21 points to an IRQ stub that iretq returns

Acceptance:
- Kernel receives IRQ1 interrupts when keys are pressed in QEMU window

### 2. Keyboard Driver Initialization

Provide:

- void kbd_init(void);

Responsibilities:
- Flush pending data from port 0x60 if status indicates data ready
- Initialize internal state:
  - shift_left, shift_right
  - caps_lock
  - ctrl, alt (optional but recommended)

No need to program controller commands in this prototype. Keep it minimal.

### 3. Scancode Handling (Set 1)

Implement scancode Set 1 decoding:

- Press event: scancode with high bit clear
- Release event: scancode | 0x80

Handle at minimum:
- Letters a-z
- Numbers 0-9
- Space, Enter, Backspace, Tab
- Left/Right Shift
- Caps Lock

Ignore or safely drop:
- Extended scancodes (0xE0 prefix) for now, but do not break if they appear

### 4. ASCII Translation

Provide:

- int kbd_translate(uint8_t scancode, int pressed);

Return:
- ASCII character for key press events
- 0 for non-printable keys or release events

Rules:
- Shift modifies letters and number row symbols
- Caps Lock toggles letter case (combined properly with Shift)
  - If shift XOR caps -> uppercase
  - Else lowercase

### 5. Input Buffer

Implement a ring buffer for received characters:

- size: 256 bytes (minimum)
- write index updated in IRQ context
- read index used by consumer code

Provide:

- int kbd_getc_nonblock(void);  // returns -1 if empty
- char kbd_getc_blocking(void); // sleeps with hlt until available

Blocking behavior:
- Must not busy-wait
- Must use hlt in a loop while checking buffer state
- Must keep interrupts enabled

### 6. Line Input Helper

Provide:

- size_t kbd_readline(char *dst, size_t max);

Behavior:
- Blocks until Enter pressed
- Echo typed characters to framebuffer console
- Supports backspace:
  - removes last character from buffer if any
  - updates console visually (erase last char)
- Terminates string with '\0'
- Returns length (excluding '\0')

Console echo:
- Use your console_putc/puts and present logic as needed

### 7. Concurrency and Safety

Because IRQ writes and kernel reads can interleave:

- Use simple interrupt disable/enable around ring buffer pop/push critical sections:
  - cli around updating shared indices
  - sti after
- Keep critical sections short

Never call kmalloc in IRQ handler.

### 8. Diagnostics

On boot print:
- "KBD: init"
- "KBD: IRQ1 enabled"

Optional debug mode:
- Print raw scancodes to serial when TEST_KBD_SCANCODE is enabled

## Validation Tests

All tests must run under QEMU with display enabled.

### TEST1: Raw Input Echo

- Print prompt on console: "Type keys, ESC to stop"
- Loop:
  - c = kbd_getc_blocking()
  - console_putc(c)
  - If c == 27 (ESC), exit test

Expected:
- Characters appear immediately as typed

### TEST2: Line Input

- Prompt: "Enter your name: "
- Call kbd_readline(buf, 64)
- Print: "Hello, <name>"

Expected:
- Backspace works
- Enter submits line

### TEST3: Modifier Keys

- Prompt user to type: "aA1! (with shift/caps)"
- Verify visually that:
  - shift modifies correctly
  - caps lock toggles letter case

## Deliverables

New files:
- include/kbd.h
- src/kbd.c

Modified:
- Makefile (add src/kbd.c)
- src/kernel.c (call kbd_init and run tests behind a flag)
- IDT/IRQ setup to register vector 0x21 stub/handler if not already present

## Acceptance Criteria

Prototype 12 is complete when:
- IRQ1 is unmasked and stable
- Key presses produce characters on screen
- Shift and Caps Lock work for letters
- Backspace and Enter work in kbd_readline
- No missed interrupts or lockups occur during rapid typing
- No ASSERT failures or page faults occur

## Next Prototype (Planned)

Prototype 13: Minimal Shell (Kernel-side)

Goals:
- Command loop using kbd_readline
- Commands:
  - help
  - clear
  - ls (root directory only, 8.3)
  - cat <file>
  - run <prog.elf> (exec user ELF from disk)
- Establish userland interaction loop

This turns the OS into an interactive system.
```
