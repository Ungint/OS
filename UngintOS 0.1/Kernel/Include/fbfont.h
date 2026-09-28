#ifndef FBFONT_H
#define FBFONT_H

#include "stdint.h"

void fbfont_draw_char(
    uint32_t *buffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitch,
    uint32_t x,
    uint32_t y,
    char c,
    uint32_t fg,
    uint32_t bg
);

void fbfont_draw_string(
    uint32_t *buffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitch,
    uint32_t x,
    uint32_t y,
    const char *str,
    uint32_t fg,
    uint32_t bg
);

#endif