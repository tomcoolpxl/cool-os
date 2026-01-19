#include <stddef.h>
#include "framebuffer.h"
#include "limine.h"
#include "hhdm.h"
#include "heap.h"
#include "serial.h"
#include "panic.h"

/* External Limine framebuffer response from kernel.c */
extern volatile struct limine_framebuffer_request framebuffer_request;

static framebuffer_t fb;
static int initialized = 0;

/* Helper to print hex number to serial */
static void fb_print_hex(uint64_t val) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(val >> i) & 0xf]);
    }
}

/* Helper to print decimal number to serial */
static void fb_print_dec(uint32_t val) {
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (val == 0) {
        serial_putc('0');
        return;
    }
    while (val > 0 && i > 0) {
        buf[--i] = '0' + (val % 10);
        val /= 10;
    }
    serial_puts(&buf[i]);
}

int fb_init(void) {
    struct limine_framebuffer_response *resp = framebuffer_request.response;

    if (resp == NULL || resp->framebuffer_count == 0) {
        serial_puts("fb: No framebuffer available\n");
        return -1;
    }

    struct limine_framebuffer *lfb = resp->framebuffers[0];

    /* Validate 32-bit format */
    if (lfb->bpp != 32) {
        serial_puts("fb: Unsupported bpp (need 32): ");
        fb_print_dec(lfb->bpp);
        serial_puts("\n");
        return -1;
    }

    /* Store hardware parameters */
    fb.hw_width = lfb->width;
    fb.hw_height = lfb->height;
    fb.hw_pitch = lfb->pitch;
    fb.hw_bpp = lfb->bpp;

    /* Debug: print raw address from Limine */
    serial_puts("fb: Limine address: ");
    fb_print_hex((uint64_t)lfb->address);
    serial_puts("\n");

    /* Check if address needs HHDM conversion (physical vs virtual) */
    uint64_t addr = (uint64_t)lfb->address;
    if (addr < 0xFFFF000000000000ULL) {
        /* Address is physical, convert via HHDM */
        serial_puts("fb: Converting physical to HHDM\n");
        fb.front = phys_to_hhdm(addr);
    } else {
        /* Address is already virtual */
        fb.front = lfb->address;
    }

    /* Use hardware resolution directly (no fixed 960x540) */
    fb.render_width = fb.hw_width;
    fb.render_height = fb.hw_height;

    /* Use front buffer directly (no double buffering) */
    fb.back = NULL;

    /* No scaling needed - render at native resolution */
    fb.scaled_width = fb.hw_width;
    fb.scaled_height = fb.hw_height;
    fb.offset_x = 0;
    fb.offset_y = 0;
    fb.scale_x_num = 1;
    fb.scale_x_den = 1;
    fb.scale_y_num = 1;
    fb.scale_y_den = 1;

    initialized = 1;

    /* Print initialization info */
    serial_puts("fb: Hardware: ");
    fb_print_dec(fb.hw_width);
    serial_puts("x");
    fb_print_dec(fb.hw_height);
    serial_puts("x");
    fb_print_dec(fb.hw_bpp);
    serial_puts(" pitch=");
    fb_print_dec(fb.hw_pitch);
    serial_puts("\n");

    serial_puts("fb: Render: ");
    fb_print_dec(fb.render_width);
    serial_puts("x");
    fb_print_dec(fb.render_height);
    serial_puts(" (direct, no scaling)\n");

    serial_puts("fb: Front buffer at ");
    fb_print_hex((uint64_t)fb.front);
    serial_puts("\n");

    /* Clear entire hardware framebuffer to black */
    uint8_t *front = (uint8_t *)fb.front;
    for (uint32_t y = 0; y < fb.hw_height; y++) {
        uint32_t *row = (uint32_t *)(front + y * fb.hw_pitch);
        for (uint32_t x = 0; x < fb.hw_width; x++) {
            row[x] = 0x00000000;
        }
    }

    serial_puts("fb: Init complete (direct rendering, no back buffer)\n");
    return 0;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!initialized || x >= fb.render_width || y >= fb.render_height) {
        return;
    }
    /* Draw directly to front buffer */
    uint8_t *front = (uint8_t *)fb.front;
    uint32_t *row = (uint32_t *)(front + y * fb.hw_pitch);
    row[x] = color;
}

void fb_clear(uint32_t color) {
    if (!initialized) return;

    /* Clear directly on front buffer */
    uint8_t *front = (uint8_t *)fb.front;
    for (uint32_t y = 0; y < fb.render_height; y++) {
        uint32_t *row = (uint32_t *)(front + y * fb.hw_pitch);
        for (uint32_t x = 0; x < fb.render_width; x++) {
            row[x] = color;
        }
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!initialized) return;

    /* Clip to render bounds */
    if (x >= fb.render_width || y >= fb.render_height) return;
    if (x + w > fb.render_width) w = fb.render_width - x;
    if (y + h > fb.render_height) h = fb.render_height - y;

    /* Draw directly to front buffer */
    uint8_t *front = (uint8_t *)fb.front;
    for (uint32_t dy = 0; dy < h; dy++) {
        uint32_t *row = (uint32_t *)(front + (y + dy) * fb.hw_pitch);
        for (uint32_t dx = 0; dx < w; dx++) {
            row[x + dx] = color;
        }
    }
}

void fb_present(void) {
    /* No-op: we draw directly to front buffer */
    (void)0;
}

const framebuffer_t *fb_get_info(void) {
    return initialized ? &fb : NULL;
}
