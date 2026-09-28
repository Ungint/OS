// Kernel/SourceI/shell.c
#include "../Include/stdint.h"
#include "../Include/keyboard.h"
#include "../Include/vga.h"
#include "../Include/string.h"
#include "../Include/shell.h"
#include "../Include/fat32.h"

#define MAX_CMD 256
#define MAX_PATH 128
#define MAX_FILE_READ 4096

// ============================================
// BIẾN TOÀN CỤC: ĐƯỜNG DẪN HIỆN TẠI
// ============================================
static char current_path[MAX_PATH] = "/";
static char current_display_path[MAX_PATH] = "/";

// ============================================
// HÀM XỬ LÝ PATH
// ============================================

static int path_exists(const char *path) {
    return fat32_chdir(path) == 0;
}

static void normalize_path(const char *input, char *output) {
    char temp[MAX_PATH];
    static char stack[MAX_PATH][MAX_PATH];
    static char buffer[MAX_PATH];
    int top = 0;
    char *token;
    int i;

    if (input[0] != '/') {
        strcpy(temp, current_path);
        if (temp[strlen(temp) - 1] != '/') {
            strcat(temp, "/");
        }
        strcat(temp, input);
        normalize_path(temp, output);
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

// ============================================
// LỆNH tp (cd)
// ============================================
static void cmd_tp(char *args) {
    char new_path[MAX_PATH];
    char normalized[MAX_PATH];

    if (args[0] == 0) {
        print("Usage: tp <path>\n", VGA_COLOR_RED, VGA_BG_BLACK);
        print("  e.g. tp /      -> go to root\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print("  e.g. tp ..     -> go up one level\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print("  e.g. tp folder -> go to folder (relative)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
        return;
    }

    if (args[0] == '/') {
        strcpy(new_path, args);
    } else {
        strcpy(new_path, current_path);
        if (current_path[strlen(current_path) - 1] != '/') {
            strcat(new_path, "/");
        }
        strcat(new_path, args);
    }
    
    normalize_path(new_path, normalized);
    
    if (!path_exists(normalized)) {
        print("tp: path not found: ", VGA_COLOR_RED, VGA_BG_BLACK);
        print(normalized, VGA_COLOR_RED, VGA_BG_BLACK);
        print("\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return;
    }
    
    strcpy(current_path, normalized);
    if (strcmp(current_path, "/") == 0) {
        strcpy(current_display_path, "/");
    } else {
        strcpy(current_display_path, current_path);
    }
    
    print("Changed to: ", VGA_COLOR_GREEN, VGA_BG_BLACK);
    print(current_display_path, VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

// ============================================
// LỆNH pwd
// ============================================
static void cmd_pwd(void) {
    print(current_display_path, VGA_COLOR_CYAN, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

// ============================================
// CÁC LỆNH KHÁC
// ============================================
static void cmd_help(void) {
    print("\n=== MYOS SHELL COMMANDS ===\n", VGA_COLOR_YELLOW, VGA_BG_BLACK);
    print("  help          - Show this help\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  cs            - Clear screen\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  out <text>    - Print text\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  off           - Shutdown OS\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  res           - Reboot OS\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  shf / ls      - Show files in current directory (FAT32)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  tp <path>     - Change directory (e.g. tp /, tp ..)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  pwd           - Show current directory\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  cat <file>    - View file content (FAT32)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  wf <file> <text> - Write text to a new file (FAT32)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  ver           - Show OS version info\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  panic         - Trigger kernel panic\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

static void cmd_cs(void) {
    clear_screen();
}

static void cmd_out(char *args) {
    print(args, VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

static void cmd_off(void) {
    print("Shutting down...\n", VGA_COLOR_RED, VGA_BG_BLACK);
    __asm__ volatile("cli");
    __asm__ volatile("outw %%ax, %%dx" : : "a"(0x2000), "d"(0x0604));
    while (1) __asm__ volatile("hlt");
}

static void cmd_res(void) {
    print("Rebooting...\n", VGA_COLOR_YELLOW, VGA_BG_BLACK);
    __asm__ volatile("cli");
    __asm__ volatile("outb %%al, %%dx" : : "a"(0xFE), "d"(0x64));
    while (1) __asm__ volatile("hlt");
}

static void cmd_shf(void) {
    fat32_ls();
}

static void cmd_cat(char *args) {
    static char file_buf[MAX_FILE_READ + 1];
    int n;

    if (args[0] == 0) {
        print("Usage: cat <file>\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return;
    }

    if (fat32_open(args) != 0) {
        print("Error: File not found: ", VGA_COLOR_RED, VGA_BG_BLACK);
        print(args, VGA_COLOR_RED, VGA_BG_BLACK);
        print("\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return;
    }

    n = fat32_read(file_buf, MAX_FILE_READ);
    if (n < 0) n = 0;
    file_buf[n] = 0;

    print(file_buf, VGA_COLOR_WHITE, VGA_BG_BLACK);
    if (n == MAX_FILE_READ) {
        print("\n[...truncated, file larger than ", VGA_COLOR_YELLOW, VGA_BG_BLACK);
        print_dec(MAX_FILE_READ);
        print(" bytes...]\n", VGA_COLOR_YELLOW, VGA_BG_BLACK);
    } else {
        print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    }

    fat32_close();
}

static void cmd_wf(char *args) {
    char filename[MAX_PATH];
    int i = 0, j = 0;

    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != 0 && j < MAX_PATH - 1) {
        filename[j++] = args[i++];
    }
    filename[j] = 0;

    while (args[i] == ' ') i++;
    char *content = &args[i];

    if (filename[0] == 0) {
        print("Usage: wf <file> <text>\n", VGA_COLOR_RED, VGA_BG_BLACK);
        print("  e.g. wf hello.txt Xin chao MYOS!\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
        return;
    }

    if (fat32_create(filename) != 0) {
        print("Error: cannot create file: ", VGA_COLOR_RED, VGA_BG_BLACK);
        print(filename, VGA_COLOR_RED, VGA_BG_BLACK);
        print("\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return;
    }

    int len = strlen(content);
    if (len > 0) {
        fat32_write(content, (uint32_t)len);
    }
    fat32_close();

    print("Wrote ", VGA_COLOR_GREEN, VGA_BG_BLACK);
    print_dec((uint32_t)len);
    print(" bytes to ", VGA_COLOR_GREEN, VGA_BG_BLACK);
    print(filename, VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

static void cmd_ver(void) {
    print("\nMYOS - a tiny hobby x86_64 OS\n", VGA_COLOR_YELLOW, VGA_BG_BLACK);
    print("Kernel:    C (freestanding, no libc)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("Boot:      2-stage (real mode -> protected mode -> long mode)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("Disk:      ATA PIO (LBA28)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("Filesystem: FAT32 (read/write, supports LFN)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("Shell:     MYOS SHELL v3.0\n\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
}

static void cmd_panic(void) {
    print("Triggering a test kernel panic in 1 second...\n", VGA_COLOR_RED, VGA_BG_BLACK);
    for (volatile long i = 0; i < 200000000; i++);
    kpanic("Manual panic requested by user via 'panic' command.");
}

static void cmd_run_urs(char *args) {
    int len = strlen(args);
    if (len < 4 || args[len-4] != '.' || args[len-3] != 'u' ||
        args[len-2] != 'r' || args[len-1] != 's') {
        print("Error: Not a .urs file!\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return;
    }
    print("Running: ", VGA_COLOR_GREEN, VGA_BG_BLACK);
    print(args, VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print("  (URS loader coming soon!)\n", VGA_COLOR_LIGHT_GRAY, VGA_BG_BLACK);
}

static void parse_command(char *input, char *cmd, char *args) {
    int i = 0, j = 0;
    
    while (input[i] != ' ' && input[i] != '\n' && input[i] != 0) {
        cmd[i] = input[i];
        i++;
    }
    cmd[i] = 0;
    
    while (input[i] == ' ') i++;
    
    while (input[i] != 0 && input[i] != '\n') {
        args[j++] = input[i++];
    }
    args[j] = 0;
}

void shell_run(void) {
    char input[MAX_CMD];
    char cmd[MAX_CMD];
    char args[MAX_CMD];
    int pos = 0;

    print("\n========================================\n", VGA_COLOR_CYAN, VGA_BG_BLACK);
    print("     MYOS SHELL v3.0\n", VGA_COLOR_YELLOW, VGA_BG_BLACK);
    print("========================================\n", VGA_COLOR_CYAN, VGA_BG_BLACK);
    print("Type 'help' for commands\n\n", VGA_COLOR_WHITE, VGA_BG_BLACK);

    while (1) {
        print("[", VGA_COLOR_BLUE, VGA_BG_BLACK);
        print(current_display_path, VGA_COLOR_GREEN, VGA_BG_BLACK);
        print("]> ", VGA_COLOR_WHITE, VGA_BG_BLACK);

        pos = 0;
        while (1) {
            char c = getch();
            
            if (c == '\n') {
                input[pos] = 0;
                print("\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
                break;
            }
            
            if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    print("\b \b", VGA_COLOR_WHITE, VGA_BG_BLACK);
                }
                continue;
            }
            
            if (c >= 32 && c < 127) {
                if (pos < MAX_CMD - 1) {
                    input[pos++] = c;
                    char str[2] = {c, 0};
                    print(str, VGA_COLOR_WHITE, VGA_BG_BLACK);
                }
            }
        }

        parse_command(input, cmd, args);

        if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "cs") == 0) {
            cmd_cs();
        } else if (strcmp(cmd, "out") == 0) {
            cmd_out(args);
        } else if (strcmp(cmd, "off") == 0) {
            cmd_off();
        } else if (strcmp(cmd, "res") == 0) {
            cmd_res();
        } else if (strcmp(cmd, "shf") == 0 || strcmp(cmd, "ls") == 0) {
            cmd_shf();
        } else if (strcmp(cmd, "tp") == 0) {
            cmd_tp(args);
        } else if (strcmp(cmd, "pwd") == 0) {
            cmd_pwd();
        } else if (strcmp(cmd, "cat") == 0) {
            cmd_cat(args);
        } else if (strcmp(cmd, "wf") == 0) {
            cmd_wf(args);
        } else if (strcmp(cmd, "ver") == 0) {
            cmd_ver();
        } else if (strcmp(cmd, "panic") == 0) {
            cmd_panic();
        } else if (cmd[0] != 0) {
            int len = strlen(cmd);
            if (len >= 4 && cmd[len-4] == '.' && cmd[len-3] == 'u' &&
                cmd[len-2] == 'r' && cmd[len-1] == 's') {
                cmd_run_urs(cmd);
            } else {
                print("Unknown command: ", VGA_COLOR_RED, VGA_BG_BLACK);
                print(cmd, VGA_COLOR_RED, VGA_BG_BLACK);
                print("\n", VGA_COLOR_RED, VGA_BG_BLACK);
            }
        }
    }
}