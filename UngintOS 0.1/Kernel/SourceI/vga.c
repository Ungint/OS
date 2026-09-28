#include "../Include/vga.h"
#include "../Include/stdint.h"

static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;
static uint8_t current_color = VGA_MAKE_COLOR(VGA_COLOR_WHITE, VGA_BG_BLACK);

// Helper function to scroll screen
static void scroll_screen(void) {
    uint16_t *vga = (uint16_t*)VGA_ADDRESS;
    
    // Move all rows up by one
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga[y * VGA_WIDTH + x] = vga[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear last row
    uint16_t blank = (current_color << 8) | ' ';
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

void vga_putchar(char c, uint8_t color, uint8_t backcolor) {
    uint16_t *vga = (uint16_t*)VGA_ADDRESS;
    
    // FIX 1: Handle backspace
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
        uint16_t blank = (VGA_MAKE_COLOR(color, backcolor) << 8) | ' ';
        vga[pos] = blank;
        return;
    }
    
    // FIX 2: Handle newline properly with scrolling
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll_screen();
            cursor_y = VGA_HEIGHT - 1;
        }
        return;
    }
    
    // FIX 3: Handle tab
    if (c == '\t') {
        do {
            vga_putchar(' ', color, backcolor);
        } while (cursor_x % 4 != 0);
        return;
    }
    
    // FIX 4: Proper attribute combining - include backcolor!
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    uint8_t attribute = VGA_MAKE_COLOR(color, backcolor);
    vga[pos] = (attribute << 8) | (unsigned char)c;
    
    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // FIX 5: Check if we need to scroll
    if (cursor_y >= VGA_HEIGHT) {
        scroll_screen();
        cursor_y = VGA_HEIGHT - 1;
    }
}

// FIX 6: Fixed print function - was missing backcolor parameter!
void print(const char *str, uint8_t color, uint8_t backcolor) {
    while (*str) {
        vga_putchar(*str++, color, backcolor);  // Now passing backcolor
    }
}

// FIX 7: Overloaded print function using current color
void print_color(const char *str) {
    while (*str) {
        vga_putchar(*str++, current_color & 0x0F, (current_color >> 4) & 0x0F);
    }
}

void clear_screen(void) {
    uint16_t *vga = (uint16_t*)VGA_ADDRESS;
    uint16_t blank = (current_color << 8) | ' ';
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = VGA_MAKE_COLOR(fg, bg);
}

// ============================================
// IN SỐ HEX / DEC (dùng cho MBR, FAT32, debug...)
// ============================================
static const char hex_digits[] = "0123456789ABCDEF";

// In một số 32-bit dạng hex, không có tiền tố "0x", luôn đủ 8 ký tự.
void print_hex(uint32_t value) {
    char buf[9];
    for (int i = 7; i >= 0; i--) {
        buf[i] = hex_digits[value & 0xF];
        value >>= 4;
    }
    buf[8] = 0;
    print(buf, VGA_COLOR_LIGHT_CYAN, VGA_BG_BLACK);
}

// In một số 8-bit dạng hex, luôn đủ 2 ký tự (VD: 0x0A -> "0A").
void print_hex8(uint8_t value) {
    char buf[3];
    buf[0] = hex_digits[(value >> 4) & 0xF];
    buf[1] = hex_digits[value & 0xF];
    buf[2] = 0;
    print(buf, VGA_COLOR_LIGHT_CYAN, VGA_BG_BLACK);
}

// In một số 32-bit dạng thập phân (không dùng sprintf vì kernel freestanding).
void print_dec(uint32_t value) {
    char buf[11]; // uint32_t tối đa 10 chữ số + null
    int i = 10;
    buf[i] = 0;

    if (value == 0) {
        buf[--i] = '0';
    } else {
        while (value > 0 && i > 0) {
            buf[--i] = '0' + (value % 10);
            value /= 10;
        }
    }
    print(&buf[i], VGA_COLOR_WHITE, VGA_BG_BLACK);
}

// ============================================
// KERNEL PANIC - lỗi không thể phục hồi
// ============================================
void kpanic(const char *msg) {
    __asm__ volatile("cli"); // Tắt hết ngắt, không cho làm gì thêm nữa

    print("\n\n*** KERNEL PANIC ***\n", VGA_COLOR_WHITE, VGA_BG_RED);
    print(msg, VGA_COLOR_YELLOW, VGA_BG_RED);
    print("\nSystem halted.\n", VGA_COLOR_WHITE, VGA_BG_RED);

    while (1) {
        __asm__ volatile("hlt");
    }
}