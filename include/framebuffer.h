#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define FB_RENDER_WIDTH  960
#define FB_RENDER_HEIGHT 540
#define FB_RENDER_SIZE   (FB_RENDER_WIDTH * FB_RENDER_HEIGHT * 4)  /* ~2 MB */

typedef struct {
    uint32_t hw_width, hw_height, hw_pitch, hw_bpp;
    void *front;
    uint32_t render_width, render_height;
    void *back;
    uint32_t back_pitch;
    uint32_t scale_x_num, scale_x_den, scale_y_num, scale_y_den;
    uint32_t offset_x, offset_y, scaled_width, scaled_height;
} framebuffer_t;

int fb_init(void);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_clear(uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_present(void);
const framebuffer_t *fb_get_info(void);

#endif
