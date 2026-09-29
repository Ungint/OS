#ifndef TERMINAL_H
#define TERMINAL_H

#include "stdint.h"

#define TERMINAL_MAX_CMD 256
#define TERMINAL_MAX_PATH 128
#define TERMINAL_MAX_LINES 128
#define TERMINAL_LINE_LEN 128
#define TERMINAL_HISTORY_MAX 16

typedef struct {
    int active;
    int canvas_id;
    
    char current_path[TERMINAL_MAX_PATH];
    
    // Line buffer for output history in terminal
    char lines[TERMINAL_MAX_LINES][TERMINAL_LINE_LEN];
    uint32_t line_colors[TERMINAL_MAX_LINES];
    int line_count;
    int scroll_offset;
    
    // Command input line
    char input_buf[TERMINAL_MAX_CMD];
    int input_len;
    int cursor_pos;
    
    // Command history
    char history[TERMINAL_HISTORY_MAX][TERMINAL_MAX_CMD];
    int history_count;
    int history_index;
    
    uint32_t text_color;
} terminal_state_t;

void terminal_init_all(void);
int terminal_open(void);
void terminal_close_by_id(int canvas_id);
void terminal_draw(int canvas_id);
void terminal_handle_key(int canvas_id, char c, uint8_t scancode);
void terminal_handle_click(int canvas_id, int rel_x, int rel_y, int click);

#endif
