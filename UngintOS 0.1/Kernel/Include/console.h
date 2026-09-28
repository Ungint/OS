// Kernel/Include/console.h
//
// "Console ảo" gắn liền với 1 process_t dạng PROCESS_TEXT_WINDOW.
// Khác với print()/vga.c (in thẳng ra toàn màn hình VGA text-mode),
// console_printf() chỉ ghi chữ vào buffer riêng của TỪNG cửa sổ, và
// buffer đó CHỈ được vẽ bên trong phần nội dung (content area) của
// đúng cửa sổ đó khi console_draw() được gọi trong process->draw.
//
// HỖ TRỢ NHIỀU CONSOLE CÙNG LÚC: mỗi process_t (mở bằng run_process())
// có 1 id riêng (0..PROCESS_MAX-1, xem process.h). Mọi hàm console_*
// (trừ console_bind/console_draw nhận thẳng process_t*) đều nhận THAM
// SỐ id ĐẦU TIÊN để biết đang thao tác lên console của cửa sổ nào.
// Nhờ vậy mở 2, 3... console cùng lúc thì mỗi cái có buffer/con trỏ
// vẽ riêng, không bị đè/ghi nhầm lẫn sang nhau nữa.
#ifndef KCONSOLE_H
#define KCONSOLE_H

#include "stdint.h"
#include "process.h"

#define CONSOLE_MAX_ROWS 64
#define CONSOLE_MAX_COLS 128

#define CONSOLE_PADDING 4

// ID không hợp lệ / chưa gắn console nào.
#define CONSOLE_INVALID_ID (-1)

// Gắn 1 console mới vào cửa sổ "win" (process_t dạng PROCESS_TEXT_WINDOW).
// Dùng chính process_t->id làm định danh của console (mỗi process id là
// 1 console riêng biệt). Tính lại số hàng/cột dựa theo kích thước cửa sổ
// và font đang dùng. Gọi lại hàm này nếu cửa sổ bị resize.
// Trả về id của console (== win->id), hoặc CONSOLE_INVALID_ID nếu lỗi.
int console_bind(process_t *win);

// Gỡ console khỏi 1 id (gọi khi đóng cửa sổ, ví dụ trong close_process()),
// giải phóng slot để id đó (hoặc 1 process khác dùng lại id này sau này)
// có thể console_bind() lại từ đầu.
void console_unbind(int id);

// Xoá sạch nội dung console của id "id" (không đụng tới khung cửa sổ).
void console_clear(int id);

// Đặt màu chữ mặc định cho các dòng in ra SAU lệnh này, trên console "id".
void console_set_color(int id, uint32_t fg);

// Ghi 1 ký tự / 1 chuỗi thô vào console "id" (không định dạng).
void console_putc(int id, char c);
void console_write(int id, const char *str);

// printf kiểu rút gọn, hỗ trợ: %d %u %i %x %X %c %s %p %%
// In vào BUFFER của console "id" (không đụng tới VGA / màn hình chính,
// và không đụng tới console của id khác).
void console_printf(int id, const char *fmt, ...);

// Giống console_printf nhưng cho phép chọn màu chữ cho lần in này.
void console_printf_color(int id, uint32_t fg, const char *fmt, ...);

// Vẽ khung cửa sổ + toàn bộ nội dung console gắn với process "p" (theo
// p->id). Dùng làm process->draw (thay cho window_draw_process khi muốn
// có nội dung text thật sự bên trong cửa sổ).
void console_draw(process_t *p);

#endif
