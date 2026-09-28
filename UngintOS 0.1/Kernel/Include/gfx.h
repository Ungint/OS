#ifndef GFX_H
#define GFX_H

#include "stdint.h"

#define GFX_TRANSPARENT 0xFFFFFFFFu

int gfx_ready(void);
uint32_t gfx_width(void);
uint32_t gfx_height(void);

int gfx_init_backbuffer(void);
int gfx_layer_init(void);

void gfx_layer_snapshot(void);

void gfx_layer_restore_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

void gfx_putpixel(uint32_t x,uint32_t y,uint32_t color);
uint32_t gfx_getpixel(uint32_t x,uint32_t y);

void gfx_fill_rect(
    uint32_t x,
    uint32_t y,
    uint32_t w,
    uint32_t h,
    uint32_t color
);

void gfx_clear(uint32_t color);

void gfx_present(void);

void gfx_present_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void gfx_composite_image(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          const uint32_t *img, uint32_t img_w, uint32_t img_h);
uint32_t *gfx_get_backbuffer(void);
void gfx_delay_ms(uint32_t ms);

#endif
