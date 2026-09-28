// Kernel/SourceI/explorer.c
#include "../Include/explorer.h"
#include "../Include/canvas.h"
#include "../Include/fat32.h"
#include "../Include/string.h"
#include "../Include/font.h"
#include "../Include/timer.h"
#include "../Include/gfx.h"

static explorer_state_t g_explorer;
static uint64_t g_last_click_time = 0;
static int g_last_clicked_item = -1;

static void int_to_str(uint32_t num, char *buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[16];
    int i = 0;
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

static void format_size(uint32_t size, char *buf) {
    if (size < 1024) {
        int_to_str(size, buf);
        strcat(buf, " B");
    } else {
        uint32_t kb = size / 1024;
        int_to_str(kb, buf);
        strcat(buf, " KB");
    }
}

void explorer_set_fps(uint32_t fps) {
    g_explorer.fps = fps;
}

void explorer_navigate(int canvas_id, const char *path) {
    if (!path || path[0] == '\0') {
        strcpy(g_explorer.current_path, "/");
    } else {
        strcpy(g_explorer.current_path, path);
    }

    g_explorer.selected_item = -1;
    g_explorer.scroll_offset = 0;
    g_explorer.item_count = 0;

    static char names[EXPLORER_MAX_ITEMS][EXPLORER_MAX_NAME];
    static int is_dir[EXPLORER_MAX_ITEMS];
    static uint32_t sizes[EXPLORER_MAX_ITEMS];

    int count = fat32_list_dir(g_explorer.current_path, &names[0][0], EXPLORER_MAX_NAME, is_dir, sizes, EXPLORER_MAX_ITEMS);
    if (count > 0) {
        int valid_count = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(names[i], ".") == 0 || strcmp(names[i], "..") == 0) {
                continue;
            }
            strcpy(g_explorer.items[valid_count].name, names[i]);
            g_explorer.items[valid_count].is_dir = is_dir[i];
            g_explorer.items[valid_count].size = sizes[i];
            valid_count++;
        }
        g_explorer.item_count = valid_count;
    }
}

void explorer_init(int canvas_id) {
    memset(&g_explorer, 0, sizeof(g_explorer));
    explorer_navigate(canvas_id, "/");
}

static void draw_folder_icon(int canvas_id, int x, int y) {
    canvas_fill_rect(canvas_id, x, y + 2, 8, 3, 0x00F0C040);
    canvas_fill_rect(canvas_id, x, y + 4, 18, 12, 0x00FFD700);
    canvas_draw_rect(canvas_id, x, y + 4, 18, 12, 0x00C09000);
}

static void draw_file_icon(int canvas_id, int x, int y) {
    canvas_fill_rect(canvas_id, x + 2, y + 1, 12, 15, 0x00FAFAFA);
    canvas_draw_rect(canvas_id, x + 2, y + 1, 12, 15, 0x00A0A0A0);
    canvas_fill_rect(canvas_id, x + 4, y + 4, 8, 2, 0x00707070);
    canvas_fill_rect(canvas_id, x + 4, y + 8, 8, 2, 0x00707070);
    canvas_fill_rect(canvas_id, x + 4, y + 12, 5, 2, 0x00707070);
}

void explorer_draw(int canvas_id) {
    int w = canvas_width(canvas_id);
    int h = canvas_height(canvas_id);

    if (w <= 0 || h <= 0) return;

    // Background
    canvas_clear(canvas_id, 0x001E1E24);

    // 1. Top Navigation Bar (Height: 40)
    canvas_fill_rect(canvas_id, 0, 0, w, 40, 0x002A2A32);
    canvas_draw_line(canvas_id, 0, 39, w, 39, 0x003A3A45);

    // Back Button [<]
    canvas_fill_rect(canvas_id, 10, 8, 30, 24, 0x003A3A48);
    canvas_draw_rect(canvas_id, 10, 8, 30, 24, 0x00555565);
    font_draw_string_canvas(canvas_id, 20, 12, "<", 0x00FFFFFF, GFX_TRANSPARENT);

    // Up Button [^]
    canvas_fill_rect(canvas_id, 45, 8, 30, 24, 0x003A3A48);
    canvas_draw_rect(canvas_id, 45, 8, 30, 24, 0x00555565);
    font_draw_string_canvas(canvas_id, 55, 12, "^", 0x00FFFFFF, GFX_TRANSPARENT);

    // Home Button [H]
    canvas_fill_rect(canvas_id, 80, 8, 30, 24, 0x003A3A48);
    canvas_draw_rect(canvas_id, 80, 8, 30, 24, 0x00555565);
    font_draw_string_canvas(canvas_id, 90, 12, "H", 0x00FFFFFF, GFX_TRANSPARENT);

    // Address Bar Input Box
    canvas_fill_rect(canvas_id, 120, 8, w - 245, 24, 0x00141418);
    canvas_draw_rect(canvas_id, 120, 8, w - 245, 24, 0x00444455);
    font_draw_string_canvas(canvas_id, 128, 12, g_explorer.current_path, 0x005DADE2, GFX_TRANSPARENT);

    // Top Right Corner FPS Counter Box
    char fps_str[32];
    strcpy(fps_str, "FPS: ");
    char fps_num[16];
    int_to_str(g_explorer.fps, fps_num);
    strcat(fps_str, fps_num);

    canvas_fill_rect(canvas_id, w - 115, 8, 105, 24, 0x0018241B);
    canvas_draw_rect(canvas_id, w - 115, 8, 105, 24, 0x002ECC71);
    font_draw_string_canvas(canvas_id, w - 105, 12, fps_str, 0x002ECC71, GFX_TRANSPARENT);

    // 2. Main File List Header (Y: 40 to 65)
    canvas_fill_rect(canvas_id, 0, 40, w, 25, 0x0025252E);
    canvas_draw_line(canvas_id, 0, 64, w, 64, 0x003A3A45);

    font_draw_string_canvas(canvas_id, 45, 45, "Name", 0x00AAAAAA, GFX_TRANSPARENT);
    font_draw_string_canvas(canvas_id, w - 300, 45, "Type", 0x00AAAAAA, GFX_TRANSPARENT);
    font_draw_string_canvas(canvas_id, w - 150, 45, "Size", 0x00AAAAAA, GFX_TRANSPARENT);

    // 3. File List Items
    int item_y = 68;
    int item_h = 28;

    for (int i = 0; i < g_explorer.item_count; i++) {
        if (item_y + item_h > h - 25) break;

        uint32_t bg_color = (i == g_explorer.selected_item) ? 0x0034495E : ((i % 2 == 0) ? 0x0022222A : 0x001E1E24);
        canvas_fill_rect(canvas_id, 5, item_y, w - 10, item_h - 2, bg_color);

        if (i == g_explorer.selected_item) {
            canvas_draw_rect(canvas_id, 5, item_y, w - 10, item_h - 2, 0x005DADE2);
        }

        // Icon
        if (g_explorer.items[i].is_dir) {
            draw_folder_icon(canvas_id, 15, item_y + 4);
        } else {
            draw_file_icon(canvas_id, 15, item_y + 4);
        }

        // Name
        uint32_t text_color = g_explorer.items[i].is_dir ? 0x00F1C40F : 0x00ECF0F1;
        font_draw_string_canvas(canvas_id, 45, item_y + 6, g_explorer.items[i].name, text_color, GFX_TRANSPARENT);

        // Type
        const char *type_str = g_explorer.items[i].is_dir ? "File Folder" : "File";
        font_draw_string_canvas(canvas_id, w - 300, item_y + 6, type_str, 0x00888888, GFX_TRANSPARENT);

        // Size
        if (!g_explorer.items[i].is_dir) {
            char size_buf[32];
            format_size(g_explorer.items[i].size, size_buf);
            font_draw_string_canvas(canvas_id, w - 150, item_y + 6, size_buf, 0x00888888, GFX_TRANSPARENT);
        }

        item_y += item_h;
    }

    // 4. Status Bar at Bottom (Height: 24)
    canvas_fill_rect(canvas_id, 0, h - 24, w, 24, 0x0018181C);
    canvas_draw_line(canvas_id, 0, h - 24, w, h - 24, 0x003A3A45);

    char status_buf[64];
    int_to_str(g_explorer.item_count, status_buf);
    strcat(status_buf, " item(s)");
    if (g_explorer.selected_item >= 0 && g_explorer.selected_item < g_explorer.item_count) {
        strcat(status_buf, " | Selected: ");
        strcat(status_buf, g_explorer.items[g_explorer.selected_item].name);
    }
    font_draw_string_canvas(canvas_id, 10, h - 18, status_buf, 0x00AAAAAA, GFX_TRANSPARENT);
}

void explorer_handle_click(int canvas_id, int rel_x, int rel_y, int click) {
    if (!click) return;

    uint64_t now = timer_ms();

    // 1. Toolbar clicks
    if (rel_y >= 8 && rel_y <= 32) {
        // Back Button
        if (rel_x >= 10 && rel_x <= 40) {
            explorer_navigate(canvas_id, "/");
            return;
        }
        // Up Button
        if (rel_x >= 45 && rel_x <= 75) {
            if (strcmp(g_explorer.current_path, "/") != 0) {
                char parent[EXPLORER_MAX_PATH];
                strcpy(parent, g_explorer.current_path);
                int len = strlen(parent);
                while (len > 1 && parent[len - 1] != '/') {
                    parent[--len] = '\0';
                }
                if (len > 1 && parent[len - 1] == '/') {
                    parent[--len] = '\0';
                }
                explorer_navigate(canvas_id, parent);
            }
            return;
        }
        // Home Button
        if (rel_x >= 80 && rel_x <= 110) {
            explorer_navigate(canvas_id, "/");
            return;
        }
    }

    // 2. File list clicks
    if (rel_y >= 68) {
        int index = (rel_y - 68) / 28;
        if (index >= 0 && index < g_explorer.item_count) {
            int is_double_click = (index == g_last_clicked_item && (now - g_last_click_time < 500));
            g_last_clicked_item = index;
            g_last_click_time = now;

            g_explorer.selected_item = index;

            if (is_double_click) {
                if (g_explorer.items[index].is_dir) {
                    char new_path[EXPLORER_MAX_PATH];
                    if (strcmp(g_explorer.current_path, "/") == 0) {
                        strcpy(new_path, "/");
                        strcat(new_path, g_explorer.items[index].name);
                    } else {
                        strcpy(new_path, g_explorer.current_path);
                        strcat(new_path, "/");
                        strcat(new_path, g_explorer.items[index].name);
                    }
                    explorer_navigate(canvas_id, new_path);
                }
            }
        }
    }
}
