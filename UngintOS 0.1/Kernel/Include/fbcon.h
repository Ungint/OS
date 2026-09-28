#ifndef FBCON_H
#define FBCON_H

#include "stdint.h"

typedef struct
{
    uint32_t *buffer;

    uint32_t width;
    uint32_t height;
    uint32_t pitch;

    uint32_t fg;
    uint32_t bg;

    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t char_width;
    uint32_t char_height;

    uint8_t visible;

    uint32_t id;

} fbcon_t;

int fbcon_init(
    fbcon_t *con,
    uint32_t *buffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitch
);

void fbcon_clear(fbcon_t *con);

void fbcon_set_color(
    fbcon_t *con,
    uint32_t fg,
    uint32_t bg
);

void fbcon_putpixel(
    fbcon_t *con,
    uint32_t x,
    uint32_t y,
    uint32_t color
);

void fbcon_putchar(
    fbcon_t *con,
    char c
);

void fbcon_print(
    fbcon_t *con,
    const char *str
);

/*
 * Gán id cho console (dùng để phân biệt nhiều console/log source).
 */
void fbcon_set_id(
    fbcon_t *con,
    uint32_t id
);

/*
 * In ra console kèm theo id, dạng: "[id] text"
 * Dành riêng cho console (không dùng chung với fbcon_print thường).
 */
void fbcon_printid(
    fbcon_t *con,
    uint32_t id,
    const char *str
);

void fbcon_newline(
    fbcon_t *con
);

void fbcon_backspace(
    fbcon_t *con
);

void fbcon_scroll(
    fbcon_t *con
);

#endif