#ifndef PROCESS_H
#define PROCESS_H

#include "stdint.h"

#define PROCESS_MAX 32

#define PROCESS_GRAPHICS_WINDOW  1
#define PROCESS_TEXT_WINDOW      2
#define PROCESS_WARNING_POPUP    3
#define PROCESS_ERROR_POPUP      4

// ============================================
// HỆ THỐNG LAYER (Z-ORDER) ĐỘNG
// ============================================
// layer NHỎ  = càng gần người dùng (vẽ SAU, tức là đè lên trên).
// layer LỚN  = càng xa/nền (vẽ TRƯỚC, tức là bị đè xuống dưới).
//
// 2 giá trị được RESERVED, không cửa sổ thường nào được đứng ở đây:
//   LAYER_MOUSE   (0)   -> con trỏ chuột, LUÔN luôn ở trên cùng.
//   LAYER_DESKTOP (999) -> nền desktop,   LUÔN luôn ở dưới cùng.
//
// Cửa sổ bình thường (PROCESS_*_WINDOW / popup) sống trong khoảng
// [LAYER_NORMAL_MIN, LAYER_NORMAL_MAX] (1..998), KHÔNG có khoảng trống:
// cửa sổ đang "trên cùng" (được click / vừa mở) luôn có layer đúng bằng
// LAYER_NORMAL_MIN (1). Xem thêm wm.h (window manager) để biết cách
// layer được gán/đẩy lên khi click chuột trái vào 1 cửa sổ.
#define LAYER_MOUSE       0
#define LAYER_NORMAL_MIN  1
#define LAYER_NORMAL_MAX  998
#define LAYER_DESKTOP     999

typedef struct process process_t;

typedef void (*process_update_t)(process_t* process);
typedef void (*process_draw_t)(process_t* process);

struct process
{
    int used;
    int id;

    int type;

    int x;
    int y;
    int width;
    int height;

    int visible;
    int focused;

    int layer; // xem "HỆ THỐNG LAYER" ở trên + wm.h

    char title[64];

    process_update_t update;
    process_draw_t draw;

    void* data;
};

void process_manager_init(void);

int run_process(
    int type,
    const char* title,
    int x,
    int y,
    int width,
    int height,
    process_update_t update,
    process_draw_t draw,
    void* data
);

void close_process(int id);

void process_update(void);
void process_draw(void);

process_t* process_get(int id);

#endif