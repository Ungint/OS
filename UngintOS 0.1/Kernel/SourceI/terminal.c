#include "../Include/terminal.h"
#include "../Include/canvas.h"
#include "../Include/fat32.h"
#include "../Include/string.h"
#include "../Include/font.h"
#include "../Include/process.h"
#include "../Include/gfx.h"
#include "../Include/timer.h"
#include "../Include/explorer.h"
#include "../Include/vga.h"
#include "../Include/memory.h"
#include "../Include/compiler.h"
#include "../Include/wm.h"

#define MAX_TERMINALS 2

static terminal_state_t g_terminal_pool[MAX_TERMINALS];

static terminal_state_t* get_terminal_by_canvas(int canvas_id) {
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (g_terminal_pool[i].active && g_terminal_pool[i].canvas_id == canvas_id) {
            return &g_terminal_pool[i];
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

void terminal_init_all(void) {
    for (int i = 0; i < MAX_TERMINALS; i++) {
        memset(&g_terminal_pool[i], 0, sizeof(terminal_state_t));
    }
}

static void terminal_scroll_if_needed(terminal_state_t *term) {
    if (term->line_count >= TERMINAL_MAX_LINES) {
        for (int i = 0; i < TERMINAL_MAX_LINES - 1; i++) {
            strcpy(term->lines[i], term->lines[i + 1]);
            term->line_colors[i] = term->line_colors[i + 1];
        }
        term->line_count = TERMINAL_MAX_LINES - 1;
    }
}

static void terminal_print_color(terminal_state_t *term, const char *str, uint32_t color) {
    if (!term || !str) return;

    char current_line[TERMINAL_LINE_LEN];
    int cur_idx = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n' || cur_idx >= TERMINAL_LINE_LEN - 1) {
            current_line[cur_idx] = '\0';
            terminal_scroll_if_needed(term);
            strcpy(term->lines[term->line_count], current_line);
            term->line_colors[term->line_count] = color;
            term->line_count++;
            cur_idx = 0;
            if (str[i] == '\n') continue;
        }
        if (str[i] != '\r') {
            current_line[cur_idx++] = str[i];
        }
    }

    if (cur_idx > 0) {
        current_line[cur_idx] = '\0';
        terminal_scroll_if_needed(term);
        strcpy(term->lines[term->line_count], current_line);
        term->line_colors[term->line_count] = color;
        term->line_count++;
    }
}

static void terminal_print(terminal_state_t *term, const char *str) {
    terminal_print_color(term, str, term->text_color ? term->text_color : 0x00ECEFF4);
}

static void terminal_draw_cb(process_t *p) {
    if (p) {
        int canvas_id = p->id;
        terminal_draw(canvas_id);
        canvas_draw(p);
    }
}

static terminal_state_t *g_active_exec_term = 0;

static void global_term_printer(const char *str, uint32_t color) {
    if (g_active_exec_term) {
        terminal_print_color(g_active_exec_term, str, color);
    }
}

void terminal_run_unr(terminal_state_t *term, const char *unr_path) {
    terminal_state_t *prev = g_active_exec_term;
    if (!term) {
        for (int i = 0; i < MAX_TERMINALS; i++) {
            if (g_terminal_pool[i].active) {
                term = &g_terminal_pool[i];
                break;
            }
        }
    }
    g_active_exec_term = term;
    run_unr_file(unr_path, global_term_printer);
    g_active_exec_term = prev;
}

// ============================================
// COMMAND EXECUTOR FOR TERMINAL
// ============================================

static void normalize_path(const char *current, const char *input, char *output) {
    char temp[TERMINAL_MAX_PATH];
    static char stack[16][TERMINAL_MAX_PATH];
    static char buffer[TERMINAL_MAX_PATH];
    int top = 0;
    char *token;
    int i;

    if (input[0] != '/') {
        strcpy(temp, current);
        if (temp[strlen(temp) - 1] != '/') {
            strcat(temp, "/");
        }
        strcat(temp, input);
        normalize_path(current, temp, output);
        return;
    }

    strcpy(buffer, input);
    token = strtok(buffer, "/");

    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            if (top > 0) top--;
        } else if (strcmp(token, ".") != 0 && token[0] != 0) {
            strcpy(stack[top++], token);
        }
        token = strtok(NULL, "/");
    }

    output[0] = '/';
    output[1] = 0;
    for (i = 0; i < top; i++) {
        if (i > 0) strcat(output, "/");
        strcat(output, stack[i]);
    }
    if (top == 0) {
        output[0] = '/';
        output[1] = 0;
    }
}

static void exec_command(terminal_state_t *term, char *cmdline) {
    while (*cmdline == ' ') cmdline++;
    if (*cmdline == 0) return;

    char cmd[64];
    char args[TERMINAL_MAX_CMD];
    int i = 0, j = 0;

    while (cmdline[i] != ' ' && cmdline[i] != 0 && i < 63) {
        cmd[i] = cmdline[i];
        i++;
    }
    cmd[i] = 0;

    while (cmdline[i] == ' ') i++;

    while (cmdline[i] != 0) {
        args[j++] = cmdline[i++];
    }
    args[j] = 0;

    if (strcmp(cmd, "help") == 0) {
        terminal_print_color(term, "=== UNGINTOS CONSOLE COMMANDS ===", 0x00EBCB8B);
        terminal_print(term, "  help            - Show available commands");
        terminal_print(term, "  ls / shf        - List files in current directory");
        terminal_print(term, "  cd / tp <path>  - Change working directory");
        terminal_print(term, "  pwd             - Display current working directory");
        terminal_print(term, "  cat <file>      - Read and display file content");
        terminal_print(term, "  echo / out <msg>- Display a message");
        terminal_print(term, "  wf <file> <text>- Create/write text file");
        terminal_print(term, "  clear / cs      - Clear terminal screen");
        terminal_print(term, "  ver             - Display UngintOS version info");
        terminal_print(term, "  neofetch        - Display system information");
        terminal_print(term, "  date / time     - Display system uptime");
        terminal_print(term, "  touch/mkfile <f>- Create an empty file");
        terminal_print(term, "  buildc <src.c>  - Compile C source file to .unr executable");
        terminal_print(term, "  calc <a op b>   - Evaluate integer math (e.g. calc 12 + 34)");
        terminal_print(term, "  off             - Shutdown OS");
        terminal_print(term, "  res             - Reboot OS");
        terminal_print(term, "  panic           - Trigger test kernel panic");
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cs") == 0) {
        term->line_count = 0;
    } else if (strcmp(cmd, "pwd") == 0) {
        terminal_print_color(term, term->current_path, 0x0088C0D0);
    } else if (strcmp(cmd, "echo") == 0 || strcmp(cmd, "out") == 0) {
        terminal_print(term, args);
    } else if (strcmp(cmd, "ver") == 0) {
        terminal_print_color(term, "UngintOS Terminal v1.0 (x86_64 Long Mode)", 0x00A3BE8C);
        terminal_print(term, "Kernel: C freestanding | GUI: VBE 1280x720 Composite WM");
    } else if (strcmp(cmd, "neofetch") == 0) {
        terminal_print_color(term, "  _  _             _   ___  ___ ", 0x0088C0D0);
        terminal_print_color(term, " | || |_ _  __ _  |_| |   \\/ __|", 0x0088C0D0);
        terminal_print_color(term, " | || | ' \\/ _` | | | | |) \\__ \\", 0x0081A1C1);
        terminal_print_color(term, "  \\__/|_||_\\__, | |_| |___/|___/", 0x005E81AC);
        terminal_print_color(term, "           |___/                ", 0x005E81AC);
        terminal_print_color(term, "--------------------------------", 0x004C566A);
        terminal_print_color(term, "OS:        UngintOS v0.1 x86_64", 0x00ECEFF4);
        terminal_print_color(term, "Arch:      x86_64 Long Mode", 0x00ECEFF4);
        terminal_print_color(term, "Display:   VBE Graphics 1280x720 32bpp", 0x00ECEFF4);
        terminal_print_color(term, "FS:        FAT32 (Read/Write, LFN)", 0x00ECEFF4);
        terminal_print_color(term, "Storage:   ATA PIO Primary Master", 0x00ECEFF4);
    } else if (strcmp(cmd, "date") == 0 || strcmp(cmd, "time") == 0) {
        uint64_t ms = timer_ms();
        uint32_t sec = (uint32_t)(ms / 1000);
        uint32_t min = sec / 60;
        uint32_t hrs = min / 60;
        sec %= 60;
        min %= 60;

        char time_str[64];
        strcpy(time_str, "Uptime: ");
        char buf[16];
        int_to_str(hrs, buf); strcat(time_str, buf); strcat(time_str, "h ");
        int_to_str(min, buf); strcat(time_str, buf); strcat(time_str, "m ");
        int_to_str(sec, buf); strcat(time_str, buf); strcat(time_str, "s");

        terminal_print_color(term, time_str, 0x00B48EAD);
    } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "shf") == 0) {
        static char names[EXPLORER_MAX_ITEMS][EXPLORER_MAX_NAME];
        static int is_dir[EXPLORER_MAX_ITEMS];
        static uint32_t sizes[EXPLORER_MAX_ITEMS];

        const char *target = (args[0] != 0) ? args : term->current_path;
        int count = fat32_list_dir(target, &names[0][0], EXPLORER_MAX_NAME, is_dir, sizes, EXPLORER_MAX_ITEMS);

        if (count < 0) {
            terminal_print_color(term, "ls: Directory not found", 0x00BF616A);
        } else {
            for (int k = 0; k < count; k++) {
                if (strcmp(names[k], ".") == 0 || strcmp(names[k], "..") == 0) continue;
                char entry_str[128];
                strcpy(entry_str, is_dir[k] ? "[DIR]  " : "[FILE] ");
                strcat(entry_str, names[k]);
                if (!is_dir[k]) {
                    strcat(entry_str, " (");
                    char sz_buf[16];
                    int_to_str(sizes[k], sz_buf);
                    strcat(entry_str, sz_buf);
                    strcat(entry_str, " B)");
                }
                terminal_print_color(term, entry_str, is_dir[k] ? 0x00EBCB8B : 0x00ECEFF4);
            }
        }
    } else if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "tp") == 0) {
        if (args[0] == 0) {
            strcpy(term->current_path, "/");
            return;
        }
        char target_norm[TERMINAL_MAX_PATH];
        normalize_path(term->current_path, args, target_norm);

        if (fat32_chdir(target_norm) == 0) {
            strcpy(term->current_path, target_norm);
        } else {
            char err[128];
            strcpy(err, "cd: No such directory: ");
            strcat(err, args);
            terminal_print_color(term, err, 0x00BF616A);
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (args[0] == 0) {
            terminal_print_color(term, "Usage: cat <filename>", 0x00BF616A);
            return;
        }
        if (fat32_open(args) == 0) {
            static char file_buf[2048];
            int bytes = fat32_read(file_buf, sizeof(file_buf) - 1);
            fat32_close();
            if (bytes >= 0) {
                file_buf[bytes] = 0;
                terminal_print(term, file_buf);
            } else {
                terminal_print_color(term, "cat: Failed to read file", 0x00BF616A);
            }
        } else {
            char err[128];
            strcpy(err, "cat: File not found: ");
            strcat(err, args);
            terminal_print_color(term, err, 0x00BF616A);
        }
    } else if (strcmp(cmd, "wf") == 0) {
        char filename[TERMINAL_MAX_PATH];
        int idx = 0, fidx = 0;
        while (args[idx] == ' ') idx++;
        while (args[idx] != ' ' && args[idx] != 0 && fidx < TERMINAL_MAX_PATH - 1) {
            filename[fidx++] = args[idx++];
        }
        filename[fidx] = 0;

        while (args[idx] == ' ') idx++;
        char *content = &args[idx];

        if (filename[0] == 0) {
            terminal_print_color(term, "Usage: wf <filename> <text>", 0x00BF616A);
            return;
        }

        if (fat32_create(filename) == 0) {
            int len = strlen(content);
            if (len > 0) fat32_write(content, (uint32_t)len);
            fat32_close();
            char msg[128];
            strcpy(msg, "Wrote file: ");
            strcat(msg, filename);
            terminal_print_color(term, msg, 0x00A3BE8C);
        } else {
            terminal_print_color(term, "wf: Error creating file", 0x00BF616A);
        }
    } else if (strcmp(cmd, "touch") == 0 || strcmp(cmd, "mkfile") == 0) {
        if (args[0] == 0) {
            terminal_print_color(term, "Usage: touch <filename>", 0x00BF616A);
            return;
        }
        if (fat32_create(args) == 0) {
            fat32_close();
            char msg[128];
            strcpy(msg, "Created file: ");
            strcat(msg, args);
            terminal_print_color(term, msg, 0x00A3BE8C);
        } else {
            terminal_print_color(term, "touch: Error creating file", 0x00BF616A);
        }
    } else if (strcmp(cmd, "buildc") == 0) {
        if (args[0] == 0) {
            terminal_print_color(term, "Usage: buildc <file.c> [-o output.unr]", 0x00BF616A);
            return;
        }
        char src_path[128];
        char out_path[128];
        int idx = 0, sidx = 0;
        while (args[idx] == ' ') idx++;
        while (args[idx] != ' ' && args[idx] != 0 && sidx < 127) {
            src_path[sidx++] = args[idx++];
        }
        src_path[sidx] = 0;

        while (args[idx] == ' ') idx++;
        if (args[idx] == '-' && args[idx+1] == 'o') {
            idx += 2;
            while (args[idx] == ' ') idx++;
            int oidx = 0;
            while (args[idx] != ' ' && args[idx] != 0 && oidx < 127) {
                out_path[oidx++] = args[idx++];
            }
            out_path[oidx] = 0;
        } else {
            strcpy(out_path, src_path);
            int len = strlen(out_path);
            if (len > 2 && out_path[len - 2] == '.' && out_path[len - 1] == 'c') {
                out_path[len - 2] = 0;
            }
            strcat(out_path, ".unr");
        }

        if (buildc_compile(src_path, out_path, "") == 0) {
            char msg[256];
            strcpy(msg, "Successfully compiled ");
            strcat(msg, src_path);
            strcat(msg, " -> ");
            strcat(msg, out_path);
            terminal_print_color(term, msg, 0x00A3BE8C);
        } else {
            terminal_print_color(term, "buildc: Compilation failed!", 0x00BF616A);
        }
    } else if (strlen(cmd) > 4 && strcmp(&cmd[strlen(cmd) - 4], ".unr") == 0) {
        terminal_run_unr(term, cmd);
    } else if (strcmp(cmd, "calc") == 0) {
        int a = 0, b = 0;
        char op = 0;
        int p = 0;
        while (args[p] == ' ') p++;
        while (args[p] >= '0' && args[p] <= '9') {
            a = a * 10 + (args[p] - '0');
            p++;
        }
        while (args[p] == ' ') p++;
        if (args[p] == '+' || args[p] == '-' || args[p] == '*' || args[p] == '/' || args[p] == '%') {
            op = args[p++];
        }
        while (args[p] == ' ') p++;
        while (args[p] >= '0' && args[p] <= '9') {
            b = b * 10 + (args[p] - '0');
            p++;
        }

        if (op == 0) {
            terminal_print_color(term, "Usage: calc <num1> <+|-|*|/|%> <num2>", 0x00BF616A);
            return;
        }

        int res = 0;
        if (op == '+') res = a + b;
        else if (op == '-') res = a - b;
        else if (op == '*') res = a * b;
        else if (op == '/') res = (b != 0) ? (a / b) : 0;
        else if (op == '%') res = (b != 0) ? (a % b) : 0;

        char res_str[64];
        strcpy(res_str, "= ");
        char num_buf[16];
        if (res < 0) {
            strcat(res_str, "-");
            res = -res;
        }
        int_to_str((uint32_t)res, num_buf);
        strcat(res_str, num_buf);
        terminal_print_color(term, res_str, 0x0088C0D0);
    } else if (strcmp(cmd, "off") == 0 || strcmp(cmd, "shutdown") == 0) {
        terminal_print_color(term, "Shutting down system...", 0x00BF616A);
        __asm__ volatile("cli");
        __asm__ volatile("outw %%ax, %%dx" : : "a"(0x2000), "d"(0x0604));
        while (1) __asm__ volatile("hlt");
    } else if (strcmp(cmd, "res") == 0 || strcmp(cmd, "reboot") == 0) {
        terminal_print_color(term, "Rebooting system...", 0x00EBCB8B);
        __asm__ volatile("cli");
        __asm__ volatile("outb %%al, %%dx" : : "a"(0xFE), "d"(0x64));
        while (1) __asm__ volatile("hlt");
    } else if (strcmp(cmd, "panic") == 0) {
        kpanic("Manual kernel panic requested from Terminal");
    } else {
        char err[128];
        strcpy(err, "Unknown command: ");
        strcat(err, cmd);
        terminal_print_color(term, err, 0x00BF616A);
    }
}

// ============================================
// TERMINAL PUBLIC API
// ============================================

int terminal_open(void) {
    int slot = -1;
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (!g_terminal_pool[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        for (int i = 0; i < MAX_TERMINALS; i++) {
            if (g_terminal_pool[i].active) {
                process_t *p = process_get(g_terminal_pool[i].canvas_id);
                if (p && p->used) {
                    p->visible = 1;
                    wm_bring_to_front(p->id);
                    for (int k = 0; k < PROCESS_MAX; k++) {
                        process_t *other = process_get(k);
                        if (other) other->focused = (other->id == p->id);
                    }
                    return g_terminal_pool[i].canvas_id;
                }
            }
        }
        return -1;
    }

    char window_title[64];
    strcpy(window_title, "UngintOS Terminal Console");

    int process_id = run_process(
        PROCESS_GRAPHICS_WINDOW,
        window_title,
        120 + slot * 40,
        100 + slot * 40,
        720,
        450,
        0,
        terminal_draw_cb,
        0
    );

    if (process_id < 0) return -1;

    process_t *win = process_get(process_id);
    if (!win) {
        close_process(process_id);
        return -1;
    }

    int canvas_id = canvas_bind(win);
    if (canvas_id == CANVAS_INVALID_ID) {
        close_process(process_id);
        return -1;
    }

    win->visible = 1;

    // Focus the new terminal process
    for (int k = 0; k < PROCESS_MAX; k++) {
        process_t *other = process_get(k);
        if (other) other->focused = (other->id == win->id);
    }

    terminal_state_t *term = &g_terminal_pool[slot];
    memset(term, 0, sizeof(terminal_state_t));
    term->active = 1;
    term->canvas_id = canvas_id;
    strcpy(term->current_path, "/");
    term->text_color = 0x00ECEFF4;

    terminal_print_color(term, "Welcome to UngintOS Terminal Console!", 0x0088C0D0);
    terminal_print_color(term, "Type 'help' to see available commands.", 0x00EBCB8B);
    terminal_print(term, "");

    return canvas_id;
}

void terminal_close_by_id(int canvas_id) {
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (g_terminal_pool[i].active && g_terminal_pool[i].canvas_id == canvas_id) {
            g_terminal_pool[i].active = 0;
            g_terminal_pool[i].canvas_id = -1;
            break;
        }
    }
}

void terminal_handle_click(int canvas_id, int rel_x, int rel_y, int click) {
    (void)canvas_id;
    (void)rel_x;
    (void)rel_y;
    (void)click;
}

void terminal_handle_key(int canvas_id, char c, uint8_t scancode) {
    terminal_state_t *term = get_terminal_by_canvas(canvas_id);
    if (!term) return;

    if (scancode == 0x48) {
        if (term->history_count > 0 && term->history_index > 0) {
            term->history_index--;
            strcpy(term->input_buf, term->history[term->history_index]);
            term->input_len = strlen(term->input_buf);
            term->cursor_pos = term->input_len;
        }
        return;
    }

    if (scancode == 0x50) {
        if (term->history_index < term->history_count - 1) {
            term->history_index++;
            strcpy(term->input_buf, term->history[term->history_index]);
            term->input_len = strlen(term->input_buf);
            term->cursor_pos = term->input_len;
        } else {
            term->history_index = term->history_count;
            term->input_buf[0] = 0;
            term->input_len = 0;
            term->cursor_pos = 0;
        }
        return;
    }

    if (scancode == 0x4B) {
        if (term->cursor_pos > 0) term->cursor_pos--;
        return;
    }

    if (scancode == 0x4D) {
        if (term->cursor_pos < term->input_len) term->cursor_pos++;
        return;
    }

    if (c == '\b') {
        if (term->cursor_pos > 0) {
            for (int i = term->cursor_pos - 1; i < term->input_len - 1; i++) {
                term->input_buf[i] = term->input_buf[i + 1];
            }
            term->cursor_pos--;
            term->input_len--;
            term->input_buf[term->input_len] = '\0';
        }
        return;
    }

    if (c == '\n' || c == '\r') {
        char echo_line[TERMINAL_MAX_CMD + TERMINAL_MAX_PATH + 10];
        strcpy(echo_line, "[ ");
        strcat(echo_line, term->current_path);
        strcat(echo_line, " ]> ");
        strcat(echo_line, term->input_buf);

        terminal_print_color(term, echo_line, 0x00A3BE8C);

        if (term->input_len > 0) {
            if (term->history_count < TERMINAL_HISTORY_MAX) {
                strcpy(term->history[term->history_count++], term->input_buf);
            } else {
                for (int h = 0; h < TERMINAL_HISTORY_MAX - 1; h++) {
                    strcpy(term->history[h], term->history[h + 1]);
                }
                strcpy(term->history[TERMINAL_HISTORY_MAX - 1], term->input_buf);
            }
            term->history_index = term->history_count;

            exec_command(term, term->input_buf);
        }

        term->input_buf[0] = 0;
        term->input_len = 0;
        term->cursor_pos = 0;
        return;
    }

    if (c >= 32 && c <= 126) {
        if (term->input_len < TERMINAL_MAX_CMD - 1) {
            for (int i = term->input_len; i > term->cursor_pos; i--) {
                term->input_buf[i] = term->input_buf[i - 1];
            }
            term->input_buf[term->cursor_pos] = c;
            term->cursor_pos++;
            term->input_len++;
            term->input_buf[term->input_len] = '\0';
        }
        return;
    }
}

void terminal_draw(int canvas_id) {
    terminal_state_t *term = get_terminal_by_canvas(canvas_id);
    if (!term) return;

    int w = canvas_width(canvas_id);
    int h = canvas_height(canvas_id);

    if (w <= 0 || h <= 0) return;

    canvas_clear(canvas_id, 0x000F0F14);

    int line_h = 16;
    int max_visible_lines = (h - 30) / line_h;
    if (max_visible_lines < 1) max_visible_lines = 1;

    int start_line = 0;
    if (term->line_count > max_visible_lines - 1) {
        start_line = term->line_count - (max_visible_lines - 1);
    }

    int y = 8;
    for (int i = start_line; i < term->line_count; i++) {
        font_draw_string_canvas(canvas_id, 10, y, term->lines[i], term->line_colors[i], GFX_TRANSPARENT);
        y += line_h;
    }

    char prompt[TERMINAL_MAX_PATH + 10];
    strcpy(prompt, "[ ");
    strcat(prompt, term->current_path);
    strcat(prompt, " ]> ");

    int prompt_len = strlen(prompt);
    int px = 10;

    font_draw_string_canvas(canvas_id, px, y, prompt, 0x0088C0D0, GFX_TRANSPARENT);
    px += prompt_len * 8;

    font_draw_string_canvas(canvas_id, px, y, term->input_buf, 0x00ECEFF4, GFX_TRANSPARENT);

    int cursor_x = px + term->cursor_pos * 8;
    canvas_fill_rect(canvas_id, cursor_x, y, 8, 14, 0x0081A1C1);
    if (term->cursor_pos < term->input_len) {
        char cur_char[2] = {term->input_buf[term->cursor_pos], 0};
        font_draw_string_canvas(canvas_id, cursor_x, y, cur_char, 0x000F0F14, GFX_TRANSPARENT);
    }
}
