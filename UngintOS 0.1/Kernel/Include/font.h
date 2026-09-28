// Kernel/Include/font.h
// ============================================
// FONT BITMAP (built-in 8x16 + file .f nạp từ FAT32 trong thư mục Font/)
// ============================================
#ifndef FONT_H
#define FONT_H

#include "stdint.h"

#define FONT_MAX_GLYPHS   256
#define FONT_MAX_HEIGHT   32
#define FONT_DIR          "/Font"

// Định dạng file font ".f" (8 byte header, little-endian, packed):
typedef struct {
    uint8_t magic[4];
    uint8_t glyph_w;
    uint8_t glyph_h;
    uint8_t first_char;
    uint8_t count;
} __attribute__((packed)) font_header_t;

// Khởi tạo module font: nạp font bitmap 8x16 nhúng sẵn làm mặc định.
void font_init(void);

// Nạp 1 file font ".f" từ FAT32 (vd "/Font/default.f")
int font_load(const char *path);

// Quét thư mục Font/ tìm file có đuôi ".f"
int font_autoload(void);

// Kích thước glyph của font đang dùng hiện tại.
uint8_t font_glyph_w(void);
uint8_t font_glyph_h(void);

// Vẽ 1 ký tự / chuỗi lên framebuffer VBE bằng font đang dùng hiện tại.
void font_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void font_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);

// Vẽ chuỗi lên Canvas ID bằng font đang dùng hiện tại.
void font_draw_string_canvas(int canvas_id, int x, int y, const char *str, uint32_t fg, uint32_t bg);

#endif
