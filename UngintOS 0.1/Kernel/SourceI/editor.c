#include "../Include/editor.h"
#include "../Include/canvas.h"
#include "../Include/fat32.h"
#include "../Include/string.h"
#include "../Include/font.h"
#include "../Include/process.h"
#include "../Include/gfx.h"

#define MAX_EDITORS 2

static editor_state_t g_editors[MAX_EDITORS];

static editor_state_t* get_editor_by_canvas(int canvas_id) {
    for (int i = 0; i < MAX_EDITORS; i++) {
        if (g_editors[i].active && g_editors[i].canvas_id == canvas_id) {
            return &g_editors[i];
        }
    }
    return 0;
}

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

void editor_init_all(void) {
    for (int i = 0; i < MAX_EDITORS; i++) {
        g_editors[i].active = 0;
        g_editors[i].canvas_id = -1;
    }
}

static void editor_draw_cb(process_t *p) {
    if (p) {
        int canvas_id = p->id;
        editor_draw(canvas_id);
        canvas_draw(p);
    }
}

int editor_open(const char *filepath) {
    for (int i = 0; i < MAX_EDITORS; i++) {
        if (g_editors[i].active && strcmp(g_editors[i].filepath, filepath) == 0) {
            return g_editors[i].canvas_id;
        }
    }

    int slot = -1;
    for (int i = 0; i < MAX_EDITORS; i++) {
        if (!g_editors[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return -1;

    char window_title[128];
    strcpy(window_title, "Text Editor - ");
    if (filepath && filepath[0]) {
        strcat(window_title, filepath);
    } else {
        strcat(window_title, "Untitled.txt");
    }

    int process_id = run_process(
        PROCESS_GRAPHICS_WINDOW,
        window_title,
        150 + slot * 30,
        80 + slot * 30,
        700,
        500,
        0,
        editor_draw_cb,
        0
    );

    if (process_id < 0) return -1;

    process_t *win = process_get(process_id);
    if (!win) return -1;

    int canvas_id = canvas_bind(win);
    if (canvas_id == CANVAS_INVALID_ID) return -1;

    editor_state_t *ed = &g_editors[slot];
    memset(ed, 0, sizeof(editor_state_t));
    ed->active = 1;
    ed->canvas_id = canvas_id;
    if (filepath) {
        strcpy(ed->filepath, filepath);
    } else {
        strcpy(ed->filepath, "/Untitled.txt");
    }

    if (filepath && filepath[0] && fat32_open(filepath) == 0) {
        int bytes = fat32_read(ed->buffer, EDITOR_MAX_BUFFER - 1);
        if (bytes > 0) {
            ed->buffer_len = bytes;
            ed->buffer[bytes] = '\0';
        } else {
            ed->buffer[0] = '\0';
            ed->buffer_len = 0;
        }
        fat32_close();
        strcpy(ed->status_msg, "File loaded.");
    } else {
        ed->buffer[0] = '\0';
        ed->buffer_len = 0;
        strcpy(ed->status_msg, "New file.");
    }

    ed->cursor_pos = ed->buffer_len;
    ed->modified = 0;
    ed->scroll_line = 0;

    return canvas_id;
}

void editor_close_by_id(int canvas_id) {
    for (int i = 0; i < MAX_EDITORS; i++) {
        if (g_editors[i].active && g_editors[i].canvas_id == canvas_id) {
            g_editors[i].active = 0;
            g_editors[i].canvas_id = -1;
            break;
        }
    }
}

void editor_save(int canvas_id) {
    editor_state_t *ed = get_editor_by_canvas(canvas_id);
    if (!ed) return;

    if (ed->filepath[0] == '\0') {
        strcpy(ed->filepath, "/file.txt");
    }

    const char *fname = ed->filepath;
    while (*fname == '/') fname++;

    if (fat32_create(fname) == 0) {
        if (ed->buffer_len > 0) {
            fat32_write(ed->buffer, ed->buffer_len);
        }
        fat32_close();
        ed->modified = 0;
        strcpy(ed->status_msg, "Saved successfully!");
    } else {
        strcpy(ed->status_msg, "Save failed!");
    }
}

void editor_handle_click(int canvas_id, int rel_x, int rel_y, int click) {
    if (!click) return;

    editor_state_t *ed = get_editor_by_canvas(canvas_id);
    if (!ed) return;

    if (rel_y >= 5 && rel_y <= 27) {
        if (rel_x >= 10 && rel_x <= 70) {
            editor_save(canvas_id);
            return;
        }
    }

    if (rel_y >= 35 && rel_y < canvas_height(canvas_id) - 20) {
        int line_idx = (rel_y - 35) / 16 + ed->scroll_line;
        int col_idx = (rel_x - 50) / 8;
        if (col_idx < 0) col_idx = 0;

        int curr_line = 0;
        int line_start = 0;
        int i = 0;

        while (i <= ed->buffer_len) {
            if (curr_line == line_idx) {
                int line_len = 0;
                while (line_start + line_len < ed->buffer_len && ed->buffer[line_start + line_len] != '\n') {
                    line_len++;
                }
                if (col_idx > line_len) col_idx = line_len;
                ed->cursor_pos = line_start + col_idx;
                return;
            }

            if (i < ed->buffer_len && ed->buffer[i] == '\n') {
                curr_line++;
                line_start = i + 1;
            }
            i++;
        }
    }
}

void editor_handle_key(int canvas_id, char c, uint8_t scancode) {
    editor_state_t *ed = get_editor_by_canvas(canvas_id);
    if (!ed) return;

    if (scancode == 0x48) {
        int curr_line = 0, curr_col = 0;
        for (int i = 0; i < ed->cursor_pos; i++) {
            if (ed->buffer[i] == '\n') {
                curr_line++;
                curr_col = 0;
            } else {
                curr_col++;
            }
        }
        if (curr_line > 0) {
            int prev_line = curr_line - 1;
            int l = 0, p_start = 0;
            for (int i = 0; i < ed->cursor_pos; i++) {
                if (ed->buffer[i] == '\n') {
                    if (l == prev_line) break;
                    l++;
                    p_start = i + 1;
                }
            }
            int p_len = 0;
            while (p_start + p_len < ed->buffer_len && ed->buffer[p_start + p_len] != '\n') {
                p_len++;
            }
            int target_col = (curr_col < p_len) ? curr_col : p_len;
            ed->cursor_pos = p_start + target_col;
        }
        return;
    }

    if (scancode == 0x50) {
        int curr_line = 0, curr_col = 0;
        for (int i = 0; i < ed->cursor_pos; i++) {
            if (ed->buffer[i] == '\n') {
                curr_line++;
                curr_col = 0;
            } else {
                curr_col++;
            }
        }
        int next_line = curr_line + 1;
        int l = 0, n_start = -1;
        if (ed->buffer_len == 0) return;
        if (l == next_line) n_start = 0;
        for (int i = 0; i < ed->buffer_len; i++) {
            if (ed->buffer[i] == '\n') {
                l++;
                if (l == next_line) {
                    n_start = i + 1;
                    break;
                }
            }
        }
        if (n_start != -1 && n_start <= ed->buffer_len) {
            int n_len = 0;
            while (n_start + n_len < ed->buffer_len && ed->buffer[n_start + n_len] != '\n') {
                n_len++;
            }
            int target_col = (curr_col < n_len) ? curr_col : n_len;
            ed->cursor_pos = n_start + target_col;
        }
        return;
    }

    if (scancode == 0x4B) {
        if (ed->cursor_pos > 0) ed->cursor_pos--;
        return;
    }

    if (scancode == 0x4D) {
        if (ed->cursor_pos < ed->buffer_len) ed->cursor_pos++;
        return;
    }

    if (c == '\b') {
        if (ed->cursor_pos > 0) {
            for (int i = ed->cursor_pos - 1; i < ed->buffer_len - 1; i++) {
                ed->buffer[i] = ed->buffer[i + 1];
            }
            ed->cursor_pos--;
            ed->buffer_len--;
            ed->buffer[ed->buffer_len] = '\0';
            ed->modified = 1;
        }
        return;
    }

    if ((c >= 32 && c <= 126) || c == '\n' || c == '\t') {
        if (c == '\t') c = ' ';
        if (ed->buffer_len < EDITOR_MAX_BUFFER - 1) {
            for (int i = ed->buffer_len; i > ed->cursor_pos; i--) {
                ed->buffer[i] = ed->buffer[i - 1];
            }
            ed->buffer[ed->cursor_pos] = c;
            ed->cursor_pos++;
            ed->buffer_len++;
            ed->buffer[ed->buffer_len] = '\0';
            ed->modified = 1;
        }
        return;
    }
}

void editor_draw(int canvas_id) {
    editor_state_t *ed = get_editor_by_canvas(canvas_id);
    if (!ed) return;

    int w = canvas_width(canvas_id);
    int h = canvas_height(canvas_id);

    if (w <= 0 || h <= 0) return;

    canvas_clear(canvas_id, 0x001B1B1F);

    canvas_fill_rect(canvas_id, 0, 0, w, 32, 0x00282830);
    canvas_draw_line(canvas_id, 0, 31, w, 31, 0x003A3A45);

    uint32_t btn_bg = ed->modified ? 0x0027AE60 : 0x003A3A48;
    canvas_fill_rect(canvas_id, 10, 5, 60, 22, btn_bg);
    canvas_draw_rect(canvas_id, 10, 5, 60, 22, 0x00555565);
    font_draw_string_canvas(canvas_id, 18, 9, "Save", 0x00FFFFFF, GFX_TRANSPARENT);

    char disp_path[128];
    strcpy(disp_path, ed->filepath);
    if (ed->modified) {
        strcat(disp_path, " *");
    }
    font_draw_string_canvas(canvas_id, 85, 9, disp_path, 0x00E0E0E0, GFX_TRANSPARENT);

    int text_y = 35;
    int line_h = 16;

    canvas_fill_rect(canvas_id, 0, text_y, 42, h - text_y - 20, 0x00141417);
    canvas_draw_line(canvas_id, 42, text_y, 42, h - 20, 0x0033333D);

    int line_num = 1;
    int col_num = 0;
    int char_x = 50;
    int char_y = text_y;

    int cursor_line = 1;
    int cursor_col = 0;
    for (int i = 0; i < ed->cursor_pos; i++) {
        if (ed->buffer[i] == '\n') {
            cursor_line++;
            cursor_col = 0;
        } else {
            cursor_col++;
        }
    }

    int i = 0;
    char line_num_str[8];
    int_to_str(1, line_num_str);
    font_draw_string_canvas(canvas_id, 8, char_y, line_num_str, 0x00666677, GFX_TRANSPARENT);

    for (i = 0; i < ed->buffer_len; i++) {
        if (i == ed->cursor_pos) {
            canvas_fill_rect(canvas_id, char_x, char_y, 2, 14, 0x0000FFCC);
        }

        char c = ed->buffer[i];
        if (c == '\n') {
            line_num++;
            col_num = 0;
            char_x = 50;
            char_y += line_h;

            if (char_y + line_h <= h - 20) {
                int_to_str(line_num, line_num_str);
                font_draw_string_canvas(canvas_id, 8, char_y, line_num_str, 0x00666677, GFX_TRANSPARENT);
            }
        } else {
            if (char_y + line_h <= h - 20 && char_x + 8 < w) {
                char str[2] = {c, '\0'};
                font_draw_string_canvas(canvas_id, char_x, char_y, str, 0x00F0F0F0, GFX_TRANSPARENT);
            }
            char_x += 8;
            col_num++;
        }
    }

    if (ed->cursor_pos == ed->buffer_len) {
        if (char_y + line_h <= h - 20) {
            canvas_fill_rect(canvas_id, char_x, char_y, 2, 14, 0x0000FFCC);
        }
    }

    canvas_fill_rect(canvas_id, 0, h - 20, w, 20, 0x00141417);
    canvas_draw_line(canvas_id, 0, h - 20, w, h - 20, 0x003A3A45);

    char status_str[128];
    strcpy(status_str, "Ln ");
    char num_buf[16];
    int_to_str(cursor_line, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, ", Col ");
    int_to_str(cursor_col + 1, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, " | ");
    strcat(status_str, ed->status_msg);

    font_draw_string_canvas(canvas_id, 10, h - 16, status_str, 0x00AAAAAA, GFX_TRANSPARENT);
}