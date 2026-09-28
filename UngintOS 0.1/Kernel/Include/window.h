#ifndef WINDOW_H
#define WINDOW_H

#include "stdint.h"
#include "process.h"

#define WINDOW_TITLEBAR_HEIGHT 32
#define WINDOW_BORDER 2

#define WINDOW_COLOR_BORDER    0x00404048
#define WINDOW_COLOR_TITLE     0x00303038
#define WINDOW_COLOR_TITLE_FOCUS 0x00404058

#define WINDOW_COLOR_BG        0x00000000

#define WINDOW_COLOR_WARNING   0x00FFFF00
#define WINDOW_COLOR_ERROR     0x00FF4444

void window_draw_frame(
    process_t* process
);

void window_draw_graphics(
    process_t* process
);

void window_draw_text(
    process_t* process
);

void window_draw_warning(
    process_t* process
);

void window_draw_error(
    process_t* process
);

void window_draw_process(
    process_t* process
);

#endif