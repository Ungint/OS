// Kernel/Include/wm.h
//
// "Window Manager" tối giản: quản lý layer (z-order) ĐỘNG cho các cửa sổ.
//
// - layer 0   = LAYER_MOUSE   -> luôn luôn ở TRÊN CÙNG (vẽ sau cùng).
// - layer 999 = LAYER_DESKTOP -> luôn luôn ở DƯỚI CÙNG (vẽ đầu tiên).
// - Cửa sổ thường dùng layer 1..998, KHÔNG có khoảng trống. Cửa sổ ở
//   layer 1 là cửa sổ "trên cùng" trong số các cửa sổ thường.
//
// Click chuột trái vào 1 cửa sổ chưa ở layer 1 -> cửa sổ đó được đưa
// lên layer 1 (wm_bring_to_front), các cửa sổ đang đứng trước nó (layer
// nhỏ hơn) bị đẩy lùi lại +1 để nhường chỗ, thứ tự tương đối giữa chúng
// được giữ nguyên (giống ngăn xếp MRU của mọi window manager thông thường).
//
// wm_composite() ghép toàn bộ layer đang có lại thành 1 khung hình:
// mảng cửa sổ được SẮP XẾP TĂNG DẦN theo layer trước (nhỏ -> lớn), sau đó
// được vẽ theo thứ tự NGƯỢC LẠI (xa/nền vẽ trước, gần/trên vẽ sau) để ra
// đúng kiểu chồng hình painter's-algorithm: desktop dưới cùng, cửa sổ
// layer 1 đè lên mọi cửa sổ khác, và chuột luôn đè lên tất cả.
#ifndef WM_H
#define WM_H

#include "process.h"

// Đưa cửa sổ "id" lên layer 1 (trên cùng trong số cửa sổ thường).
// Nếu đã ở layer 1 thì không làm gì. Trả về layer mới (luôn là
// LAYER_NORMAL_MIN nếu id hợp lệ), hoặc -1 nếu id không hợp lệ.
// Cũng được gọi tự động bên trong run_process() khi 1 cửa sổ mới mở ra,
// để cửa sổ mới luôn xuất hiện trên cùng như mong đợi.
int wm_bring_to_front(int id);

// Gọi TRƯỚC khi đóng 1 cửa sổ (trong close_process()) để dồn (compact)
// lại các layer phía sau nó, tránh để lại "lỗ hổng" trong dải 1..998.
void wm_release_layer(int id);

// Gọi mỗi frame (sau mouse_poll()) để xử lý click chuột trái: tự dò
// cạnh lên (rising edge) của nút trái, hit-test từ cửa sổ layer 1 ra
// dần, nếu trúng 1 cửa sổ thì focus + wm_bring_to_front() cửa sổ đó;
// nếu click ra ngoài mọi cửa sổ (trúng desktop) thì bỏ focus hết.
void wm_handle_mouse(int mx, int my, int left_button);

// Ghép & vẽ 1 khung hình hoàn chỉnh theo đúng thứ tự layer:
//   desktop_draw() (layer 999, luôn vẽ đầu tiên/dưới cùng, có thể NULL)
//   -> mọi cửa sổ thường, xa nhất (layer lớn) vẽ trước, gần nhất
//      (layer 1) vẽ sau cùng trong nhóm cửa sổ
//   -> mouse_draw() (layer 0, luôn vẽ cuối cùng/trên cùng, có thể NULL)
void wm_composite(void (*desktop_draw)(void), void (*mouse_draw)(void));

#endif
