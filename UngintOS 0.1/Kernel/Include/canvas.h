// Kernel/Include/canvas.h
//
// "Canvas" gắn liền với 1 process_t dạng PROCESS_GRAPHICS_WINDOW.
// Giống console.c (buffer chữ riêng cho từng cửa sổ text), canvas.c
// cấp cho MỖI cửa sổ đồ hoạ 1 buffer PIXEL riêng (RAM, không phải
// framebuffer thật), user vẽ tuỳ ý lên buffer đó bằng canvas_*(), rồi
// canvas_draw() (dùng làm process->draw) sẽ tự vẽ khung cửa sổ + blit
// đúng buffer đó vào ĐÚNG vị trí cửa sổ trên màn hình mỗi frame.
//
// ID KHÔNG ĐƯỢC TRÙNG: mỗi canvas được lưu ở slot g_canvases[id] với
// id chính là process_t->id (0..PROCESS_MAX-1). Vì process.c chỉ cấp
// phát id qua 1 slot "used" tại 1 thời điểm (run_process tìm slot rảnh
// đầu tiên), nên 2 cửa sổ đang mở KHÔNG BAO GIỜ trùng id -> KHÔNG BAO
// GIỜ trùng canvas. Khi đóng cửa sổ, close_process() gọi canvas_unbind()
// để "trả slot" sạch sẽ; nếu 1 id cũ được tái sử dụng cho cửa sổ mới,
// canvas_bind() sẽ cấp phát lại buffer mới tinh cho id đó (không dùng
// nhầm dữ liệu/kích thước của cửa sổ cũ).
#ifndef CANVAS_H
#define CANVAS_H

#include "stdint.h"
#include "process.h"

#define CANVAS_INVALID_ID (-1)

// Gắn 1 canvas mới vào cửa sổ "win" (process_t dạng PROCESS_GRAPHICS_WINDOW,
// nhưng dùng được cho type nào cũng được nếu muốn). Cấp phát (malloc) 1
// buffer pixel kích thước đúng bằng content-area hiện tại của cửa sổ
// (bên trong viền, dưới thanh tiêu đề), xoá về màu đen.
// Trả về id của canvas (== win->id), hoặc CANVAS_INVALID_ID nếu lỗi.
// Gọi lại hàm này nếu cửa sổ bị resize (sẽ cấp buffer mới đúng size mới).
int canvas_bind(process_t *win);

// Gỡ canvas khỏi 1 id (gọi khi đóng cửa sổ, đã tự động nối vào
// close_process() trong process.c). Buffer cũ không bị free() thật sự
// (kernel dùng bump-allocator, free() là no-op) nhưng slot được đánh
// dấu "chưa bind" để id có thể canvas_bind() lại từ đầu an toàn.
void canvas_unbind(int id);

int canvas_width(int id);
int canvas_height(int id);

// Xoá toàn bộ canvas "id" về 1 màu.
void canvas_clear(int id, uint32_t color);

// Toạ độ (x,y) tính theo hệ toạ độ RIÊNG của canvas (0,0 = góc trên-trái
// vùng nội dung cửa sổ), KHÔNG phải toạ độ màn hình.
void canvas_put_pixel(int id, int x, int y, uint32_t color);
uint32_t canvas_get_pixel(int id, int x, int y);

void canvas_fill_rect(int id, int x, int y, int w, int h, uint32_t color);
void canvas_draw_rect(int id, int x, int y, int w, int h, uint32_t color);

void canvas_draw_line(int id, int x0, int y0, int x1, int y1, uint32_t color);

void canvas_draw_circle(int id, int cx, int cy, int r, uint32_t color);
void canvas_fill_circle(int id, int cx, int cy, int r, uint32_t color);

// Vẽ khung cửa sổ + blit toàn bộ nội dung canvas gắn với process "p"
// (theo p->id) vào đúng vị trí cửa sổ trên màn hình. Dùng làm
// process->draw (thay cho window_draw_process).
void canvas_draw(process_t *p);

#endif
