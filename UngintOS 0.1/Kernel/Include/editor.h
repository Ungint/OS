#ifndef EDITOR_H
#define EDITOR_H

#include "stdint.h"

#define EDITOR_MAX_BUFFER 8192
#define EDITOR_MAX_PATH 128

typedef struct {
    int active;
    int canvas_id;
    char filepath[EDITOR_MAX_PATH];
    char buffer[EDITOR_MAX_BUFFER];
    int buffer_len;
    int cursor_pos;
    int modified;
    int scroll_line;
    char status_msg[64];
} editor_state_t;

void editor_init_all(void);
int editor_open(const char *filepath);
void editor_close_by_id(int canvas_id);
void editor_draw(int canvas_id);
void editor_handle_click(int canvas_id, int rel_x, int rel_y, int click);
void editor_handle_key(int canvas_id, char c, uint8_t scancode);
void editor_save(int canvas_id);

#endif