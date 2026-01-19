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

    /* Allocate back buffer in regular RAM for fast operations */
    fb.back_pitch = fb.render_width * 4;  /* Tightly packed */
    uint64_t back_size = (uint64_t)fb.back_pitch * fb.render_height;
    fb.back = kmalloc(back_size);
    if (fb.back == NULL) {
        serial_puts("fb: Failed to allocate back buffer (");
        fb_print_dec((uint32_t)(back_size / 1024));
        serial_puts(" KB), using direct mode\n");
    } else {
        serial_puts("fb: Allocated back buffer (");
        fb_print_dec((uint32_t)(back_size / 1024));
        serial_puts(" KB) at ");
        fb_print_hex((uint64_t)fb.back);
        serial_puts("\n");
    }

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
    serial_puts(fb.back ? " (double-buffered)\n" : " (direct mode)\n");

    serial_puts("fb: Front buffer at ");
    fb_print_hex((uint64_t)fb.front);
    serial_puts("\n");

    /* Clear framebuffer to black */
    fb_clear(0x00000000);
    fb_present();

    serial_puts("fb: Init complete\n");
    return 0;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!initialized || x >= fb.render_width || y >= fb.render_height) {
        return;
    }
    if (fb.back) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb.back + y * fb.back_pitch);
        row[x] = color;
    } else {
        uint32_t *row = (uint32_t *)((uint8_t *)fb.front + y * fb.hw_pitch);
        row[x] = color;
    }
}

void fb_clear(uint32_t color) {
    if (!initialized) return;

    void *target = fb.back ? fb.back : fb.front;
    uint32_t pitch = fb.back ? fb.back_pitch : fb.hw_pitch;

    /* Use 64-bit writes for speed (2 pixels at a time) */
    uint64_t color_pair = ((uint64_t)color << 32) | color;
    for (uint32_t y = 0; y < fb.render_height; y++) {
        uint64_t *row = (uint64_t *)((uint8_t *)target + y * pitch);
        uint32_t pairs = fb.render_width / 2;
        for (uint32_t x = 0; x < pairs; x++) {
            row[x] = color_pair;
        }
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!initialized) return;

    /* Clip to render bounds */
    if (x >= fb.render_width || y >= fb.render_height) return;
    if (x + w > fb.render_width) w = fb.render_width - x;
    if (y + h > fb.render_height) h = fb.render_height - y;

    void *target = fb.back ? fb.back : fb.front;
    uint32_t pitch = fb.back ? fb.back_pitch : fb.hw_pitch;

    for (uint32_t dy = 0; dy < h; dy++) {
        uint32_t *row = (uint32_t *)((uint8_t *)target + (y + dy) * pitch);
        for (uint32_t dx = 0; dx < w; dx++) {
            row[x + dx] = color;
        }
    }
}

void fb_present(void) {
    if (!initialized || !fb.back) return;

    uint8_t *src = (uint8_t *)fb.back;
    uint8_t *dst = (uint8_t *)fb.front;
    uint32_t row_bytes = fb.render_width * 4;

    /* Copy row by row using 64-bit transfers */
    for (uint32_t y = 0; y < fb.render_height; y++) {
        uint64_t *s = (uint64_t *)(src + y * fb.back_pitch);
        uint64_t *d = (uint64_t *)(dst + y * fb.hw_pitch);
        uint32_t words = row_bytes / 8;
        for (uint32_t i = 0; i < words; i++) {
            d[i] = s[i];
        }
    }
}

const framebuffer_t *fb_get_info(void) {
    return initialized ? &fb : NULL;
}
