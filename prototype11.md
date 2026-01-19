## Prototype 11: Framebuffer Text Console (Next Step)

### Goal

Provide a software-rendered text console using the framebuffer.

This is NOT VGA text mode. It is:

* Bitmap font rendering
* Character grid
* Cursor control
* Scrolling
* Newline handling

---

### What you will implement

#### 1) Bitmap font

Choose:

* 8x16 font (recommended)
* Or 8x8 for more lines

Store as:

* uint8_t font[256][16] (for 8x16)

Each byte = one row bitmask.

---

#### 2) Console state

Track:

* Cursor X, Y (character coordinates)
* Columns = framebuffer_width / font_width
* Rows = framebuffer_height / font_height

---

#### 3) Character drawing

Implement:

* console_putc(char c)
* console_puts(const char *s)

Rendering logic:

* Translate character to glyph
* Draw pixels via fb_putpixel
* Advance cursor
* Handle newline, carriage return, tab

---

#### 4) Scrolling

When cursor reaches bottom:

* Move framebuffer content up by one character row
* Clear bottom row
* Update cursor

Use:

* memmove on framebuffer rows (not pixel-by-pixel)

---

#### 5) Clear screen

Implement:

* console_clear()

Calls:

* fb_clear(background_color)
* Reset cursor

---

#### 6) Panic screen

Update panic() to:

* Clear screen
* Print error text using framebuffer console
* Halt CPU

Serial remains as secondary output.

---

### Why this is next

You now have:

* Storage
* Programs
* Graphics

You need:

* Visible diagnostics
* On-screen shell later
* User program output redirection

Framebuffer console is mandatory infrastructure.

---

### What NOT to do yet

Do not implement:

* Window manager
* Mouse
* UI toolkit
* Fancy fonts
* Anti-aliasing

Keep it simple.

---
