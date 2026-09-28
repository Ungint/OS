#include "../Include/gfx.h"
#include "../Include/boot_data.h"
#include "../Include/memory.h"
#include "../Include/string.h"

static uint32_t *back_buffer = 0;
static uint32_t *bg_layer = 0;

int gfx_ready(void) {
    return vbe_ok ? 1 : 0;
}

uint32_t gfx_width(void)  { return vbe_ok ? boot_width  : 0; }
uint32_t gfx_height(void) { return vbe_ok ? boot_height : 0; }

int gfx_init_backbuffer(void) {
    uint32_t size;

    if (!vbe_ok) return 0;
    if (back_buffer) return 1;

    size = boot_width * boot_height * 4;
    back_buffer = (uint32_t*)malloc(size);

    return back_buffer ? 1 : 0;
}

// ============================================
// LAYER NỀN — xem giải thích trong gfx.h
// ============================================
int gfx_layer_init(void) {
    uint32_t size;

    if (!vbe_ok || !back_buffer) return 0;
    if (bg_layer) return 1;

    size = boot_width * boot_height * 4;
    bg_layer = (uint32_t*)malloc(size);

    return bg_layer ? 1 : 0;
}

void gfx_layer_snapshot(void) {
    if (!vbe_ok || !back_buffer || !bg_layer) return;
    memcpy(bg_layer, back_buffer, boot_width * boot_height * 4);
}

void gfx_layer_restore_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t end_x, end_y, row, row_bytes;

    if (!vbe_ok || !back_buffer || !bg_layer) return;
    if (x >= boot_width || y >= boot_height) return;

    end_x = x + w;
    if (end_x > boot_width) end_x = boot_width;
    end_y = y + h;
    if (end_y > boot_height) end_y = boot_height;

    row_bytes = (end_x - x) * 4;

    for (row = y; row < end_y; row++) {
        uint32_t off = (row * boot_width + x) * 4;
        memcpy((uint8_t*)back_buffer + off, (uint8_t*)bg_layer + off, row_bytes);
    }
}

void gfx_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!vbe_ok || !back_buffer) return;
    if (x >= boot_width || y >= boot_height) return;
    back_buffer[y * boot_width + x] = color;
}

uint32_t gfx_getpixel(uint32_t x, uint32_t y) {
    if (!vbe_ok || !back_buffer) return 0;
    if (x >= boot_width || y >= boot_height) return 0;
    return back_buffer[y * boot_width + x];
}

// ============================================
// FILL RECT — dùng rep stosl mỗi hàng
// ============================================
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t end_x, end_y, row;

    if (!vbe_ok || !back_buffer) return;
    if (x >= boot_width || y >= boot_height) return;

    end_x = x + w;
    if (end_x > boot_width)  end_x = boot_width;

    end_y = y + h;
    if (end_y > boot_height) end_y = boot_height;

    for (row = y; row < end_y; row++) {
        uint32_t *dst = &back_buffer[row * boot_width + x];
        uint32_t count = end_x - x;
        __asm__ volatile(
            "rep stosl"
            : "+D"(dst), "+c"(count)
            : "a"(color)
            : "memory"
        );
    }
}

// ============================================
// CLEAR — dùng rep stosl 1 phát
// ============================================
void gfx_clear(uint32_t color) {
    if (!vbe_ok || !back_buffer) return;

    uint32_t *dst = back_buffer;
    uint32_t count = boot_width * boot_height;

    __asm__ volatile(
        "rep stosl"
        : "+D"(dst), "+c"(count)
        : "a"(color)
        : "memory"
    );
}

// ============================================
// PRESENT — memcpy 1 phát nếu pitch == width*4
// ============================================
void gfx_present(void) {
    uint8_t *fb;
    uint8_t *bb;

    if (!vbe_ok || !back_buffer) return;

    fb = (uint8_t*)(uintptr_t)boot_fb;
    bb = (uint8_t*)back_buffer;

    uint32_t row_size = boot_width * 4;

    // Trường hợp lý tưởng: pitch == width*4 → copy 1 phát
    if (boot_pitch == row_size) {
        memcpy(fb, bb, row_size * boot_height);
        return;
    }

    // Có padding giữa các hàng → copy từng hàng
    for (uint32_t y = 0; y < boot_height; y++) {
        memcpy(fb + y * boot_pitch, bb + y * row_size, row_size);
    }
}

// ============================================
// PRESENT RECT — chỉ copy 1 vùng nhỏ (dirty rect) ra VRAM thật.
// Nhanh hơn gfx_present() rất nhiều khi mỗi frame chỉ có 1 vùng nhỏ
// thay đổi (vd game 2D, con trỏ, box di chuyển...).
// ============================================
void gfx_present_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint8_t *fb;
    uint8_t *bb;
    uint32_t end_x, end_y, row, row_bytes;

    if (!vbe_ok || !back_buffer) return;
    if (x >= boot_width || y >= boot_height) return;

    end_x = x + w;
    if (end_x > boot_width) end_x = boot_width;
    end_y = y + h;
    if (end_y > boot_height) end_y = boot_height;

    fb = (uint8_t*)(uintptr_t)boot_fb;
    bb = (uint8_t*)back_buffer;
    row_bytes = (end_x - x) * 4;

    for (row = y; row < end_y; row++) {
        uint32_t off_bb = (row * boot_width + x) * 4;
        uint32_t off_fb = row * boot_pitch + x * 4;
        memcpy(fb + off_fb, bb + off_bb, row_bytes);
    }
}

// ============================================
// 🔥 THÊM: COMPOSITE 1-PASS - xem giải thích chi tiết trong gfx.h.
// Đọc NỀN GỐC từ bg_layer (không phải back_buffer hiện tại - tránh
// trường hợp cursor lần trước còn ám lại nếu lỡ blend nhầm nguồn), blend
// alpha rồi ghi thẳng vào back_buffer, tất cả trong 1 vòng lặp duy nhất.
// ============================================
void gfx_composite_image(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          const uint32_t *img, uint32_t img_w, uint32_t img_h) {
    if (!vbe_ok || !back_buffer || !bg_layer || !img) return;
    if (x >= boot_width || y >= boot_height) return;

    uint32_t end_x = x + w;
    if (end_x > boot_width) end_x = boot_width;
    uint32_t end_y = y + h;
    if (end_y > boot_height) end_y = boot_height;

    for (uint32_t dy = y; dy < end_y; dy++) {
        uint32_t sy = dy - y;
        if (sy >= img_h) break;

        uint32_t *bb_row = &back_buffer[dy * boot_width];
        const uint32_t *bg_row = &bg_layer[dy * boot_width];
        const uint32_t *img_row = &img[sy * img_w];

        for (uint32_t dx = x; dx < end_x; dx++) {
            uint32_t sx = dx - x;
            if (sx >= img_w) break;

            uint32_t sp = img_row[sx];
            uint8_t a = (uint8_t)(sp >> 24);

            if (a == 0) {
                bb_row[dx] = bg_row[dx];   // trong suốt hoàn toàn -> y hệt nền gốc
                continue;
            }
            if (a == 255) {
                bb_row[dx] = sp & 0x00FFFFFFu;
                continue;
            }

            uint32_t dp = bg_row[dx];
            uint8_t sr = (uint8_t)(sp >> 16), sg = (uint8_t)(sp >> 8), sb = (uint8_t)sp;
            uint8_t dr = (uint8_t)(dp >> 16), dg = (uint8_t)(dp >> 8), db = (uint8_t)dp;

            uint8_t orr = (uint8_t)(((uint32_t)sr * a + (uint32_t)dr * (255 - a)) / 255);
            uint8_t og  = (uint8_t)(((uint32_t)sg * a + (uint32_t)dg * (255 - a)) / 255);
            uint8_t ob  = (uint8_t)(((uint32_t)sb * a + (uint32_t)db * (255 - a)) / 255);

            bb_row[dx] = ((uint32_t)orr << 16) | ((uint32_t)og << 8) | ob;
        }
    }
}

// ============================================
// DELAY — dùng timer thật (nếu có timer.h)
// ============================================// Tạm giữ nguyên, nhưng khuyên bro chuyển sang timer_ms()
void gfx_delay_ms(uint32_t ms) {
    uint32_t i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 200000; j++) {
            __asm__ volatile("nop");
        }
    }
}

uint32_t *gfx_get_backbuffer(void)
{
    return back_buffer;
}