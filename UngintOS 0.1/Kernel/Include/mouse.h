// Kernel/Include/mouse.h
// ============================================
// DRIVER CHUỘT PS/2 (polling, giống style keyboard.c - không dùng IRQ12)
// ============================================
#ifndef MOUSE_H
#define MOUSE_H

#include "stdint.h"

// Khởi tạo controller 8042 cho cổng AUX (chuột): bật clock cổng 2, gửi
// lệnh 0xF6 (set defaults) + 0xF4 (enable data reporting) tới chuột.
// Truyền vào kích thước màn hình để driver tự kẹp (clamp) toạ độ con trỏ
// trong vùng nhìn thấy được. Gọi 1 lần lúc boot, SAU keyboard_init().
void mouse_init(uint32_t screen_w, uint32_t screen_h);

// Đọc hết các byte PS/2 chuột đang có sẵn trong buffer (nếu có) và cập
// nhật toạ độ/trạng thái nút. Không chặn (non-blocking) - gọi mỗi frame.
void mouse_poll(void);

// Toạ độ con trỏ hiện tại (đã kẹp trong màn hình).
int mouse_x(void);
int mouse_y(void);

// Trạng thái nút chuột (1 = đang nhấn).
int mouse_left_button(void);
int mouse_right_button(void);
int mouse_middle_button(void);

#endif
