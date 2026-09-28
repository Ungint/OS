#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_ADDRESS  0xB8000
#define VGA_WIDTH    80
#define VGA_HEIGHT   25

// Foreground colors
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GRAY    0x7
#define VGA_COLOR_DARK_GRAY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

// Background colors
#define VGA_BG_BLACK      0x0
#define VGA_BG_BLUE       0x1
#define VGA_BG_GREEN      0x2
#define VGA_BG_CYAN       0x3
#define VGA_BG_RED        0x4
#define VGA_BG_MAGENTA    0x5
#define VGA_BG_BROWN      0x6
#define VGA_BG_LIGHT_GRAY 0x7

// Macro tạo màu
#define VGA_MAKE_COLOR(fg, bg) (((bg) << 4) | (fg))

// Các hàm VGA
void vga_putchar(char c, uint8_t color, uint8_t backcolor);
void print(const char *str, uint8_t color, uint8_t backcolor);
void print_color(const char *str);
void clear_screen(void);
void vga_set_color(uint8_t fg, uint8_t bg);

// Helpers in số hex/dec - dùng cho debug (MBR, FAT32, ...)
void print_hex(uint32_t value);
void print_hex8(uint8_t value);
void print_dec(uint32_t value);

// Báo lỗi nghiêm trọng và treo máy (dùng khi driver gặp lỗi không phục hồi được)
void kpanic(const char *msg);

#endif