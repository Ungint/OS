// Kernel/Include/explorer.h
#ifndef EXPLORER_H
#define EXPLORER_H

#include "stdint.h"

#define EXPLORER_MAX_ITEMS 64
#define EXPLORER_MAX_PATH 128
#define EXPLORER_MAX_NAME 64

typedef struct {
    char name[EXPLORER_MAX_NAME];
    int is_dir;
    uint32_t size;
} explorer_item_t;

typedef struct {
    char current_path[EXPLORER_MAX_PATH];
    explorer_item_t items[EXPLORER_MAX_ITEMS];
    int item_count;
    int selected_item;
    int scroll_offset;
    uint32_t fps;
} explorer_state_t;

void explorer_init(int canvas_id);
void explorer_navigate(int canvas_id, const char *path);
void explorer_draw(int canvas_id);
void explorer_handle_click(int canvas_id, int rel_x, int rel_y, int click);
void explorer_set_fps(uint32_t fps);

#endif
