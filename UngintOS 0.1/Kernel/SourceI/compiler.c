#include "../Include/compiler.h"
#include "../Include/fat32.h"
#include "../Include/string.h"
#include "../Include/memory.h"

#define UNR_MAGIC "UNR1"
#define MAX_SRC_SIZE 16384
#define MAX_TOKENS 1024
#define MAX_NODES 512

typedef enum {
    TOK_INT, TOK_IDENT, TOK_NUMBER, TOK_STRING,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_ASSIGN,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_SEMICOLON,
    TOK_IF, TOK_WHILE, TOK_FOR, TOK_PRINTF, TOK_RETURN,
    TOK_EOF
} token_type_t;

typedef struct {
    token_type_t type;
    char text[64];
    int val;
} token_t;

static token_t tokens[MAX_TOKENS];
static int token_count = 0;
static int tok_idx = 0;

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static void my_strncpy(char *dest, const char *src, int n) {
    int i = 0;
    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i] = '\0';
        i++;
    }
}

static void lex(const char *src) {
    token_count = 0;
    int i = 0;
    while (src[i] != '\0' && token_count < MAX_TOKENS - 1) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            i++;
            continue;
        }
        if (c == '/' && src[i + 1] == '/') {
            while (src[i] != '\0' && src[i] != '\n') i++;
            continue;
        }
        if (is_alpha(c)) {
            int start = i;
            while (is_alpha(src[i]) || is_digit(src[i])) i++;
            int len = i - start;
            if (len > 63) len = 63;
            char buf[64];
            my_strncpy(buf, &src[start], len);
            buf[len] = '\0';

            tokens[token_count].val = 0;
            strcpy(tokens[token_count].text, buf);

            if (strcmp(buf, "int") == 0) tokens[token_count].type = TOK_INT;
            else if (strcmp(buf, "if") == 0) tokens[token_count].type = TOK_IF;
            else if (strcmp(buf, "while") == 0) tokens[token_count].type = TOK_WHILE;
            else if (strcmp(buf, "for") == 0) tokens[token_count].type = TOK_FOR;
            else if (strcmp(buf, "printf") == 0) tokens[token_count].type = TOK_PRINTF;
            else if (strcmp(buf, "return") == 0) tokens[token_count].type = TOK_RETURN;
            else tokens[token_count].type = TOK_IDENT;

            token_count++;
            continue;
        }
        if (is_digit(c)) {
            int val = 0;
            while (is_digit(src[i])) {
                val = val * 10 + (src[i] - '0');
                i++;
            }
            tokens[token_count].type = TOK_NUMBER;
            tokens[token_count].val = val;
            tokens[token_count].text[0] = '\0';
            token_count++;
            continue;
        }
        if (c == '"') {
            i++;
            int start = i;
            while (src[i] != '\0' && src[i] != '"') i++;
            int len = i - start;
            if (len > 63) len = 63;
            my_strncpy(tokens[token_count].text, &src[start], len);
            tokens[token_count].text[len] = '\0';
            tokens[token_count].type = TOK_STRING;
            if (src[i] == '"') i++;
            token_count++;
            continue;
        }

        tokens[token_count].text[0] = c;
        tokens[token_count].text[1] = '\0';
        tokens[token_count].val = 0;

        if (c == '+') tokens[token_count].type = TOK_PLUS;
        else if (c == '-') tokens[token_count].type = TOK_MINUS;
        else if (c == '*') tokens[token_count].type = TOK_STAR;
        else if (c == '/') tokens[token_count].type = TOK_SLASH;
        else if (c == '=') {
            if (src[i + 1] == '=') { tokens[token_count].type = TOK_EQ; i++; }
            else tokens[token_count].type = TOK_ASSIGN;
        }
        else if (c == '!') {
            if (src[i + 1] == '=') { tokens[token_count].type = TOK_NEQ; i++; }
        }
        else if (c == '<') tokens[token_count].type = TOK_LT;
        else if (c == '>') tokens[token_count].type = TOK_GT;
        else if (c == '(') tokens[token_count].type = TOK_LPAREN;
        else if (c == ')') tokens[token_count].type = TOK_RPAREN;
        else if (c == '{') tokens[token_count].type = TOK_LBRACE;
        else if (c == '}') tokens[token_count].type = TOK_RBRACE;
        else if (c == ';') tokens[token_count].type = TOK_SEMICOLON;

        i++;
        token_count++;
    }
    tokens[token_count].type = TOK_EOF;
}

int buildc_compile(const char *src_path, const char *out_path, const char *flags) {
    (void)flags;
    if (!src_path || !out_path) return -1;

    if (fat32_open(src_path) != 0) return -1;

    static char src_buf[MAX_SRC_SIZE];
    int bytes = fat32_read(src_buf, MAX_SRC_SIZE - 1);
    fat32_close();

    if (bytes <= 0) return -1;
    src_buf[bytes] = '\0';

    lex(src_buf);

    if (fat32_create(out_path) != 0) return -1;

    // Header: Magic "UNR1"
    fat32_write(UNR_MAGIC, 4);

    // Save token count & payload
    fat32_write(&token_count, sizeof(int));
    fat32_write(tokens, token_count * sizeof(token_t));

    // Save raw source
    fat32_write(&bytes, sizeof(int));
    fat32_write(src_buf, bytes);

    fat32_close();
    return 0;
}

static void int_to_str(int num, char *buf) {
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[16];
    int i = 0, neg = 0;
    if (num < 0) { neg = 1; num = -num; }
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) buf[j++] = temp[--i];
    buf[j] = '\0';
}

typedef struct {
    char name[32];
    int val;
} var_t;

static var_t vars[64];
static int var_cnt = 0;

static int get_var(const char *name) {
    for (int i = 0; i < var_cnt; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].val;
    }
    return 0;
}

static void set_var(const char *name, int val) {
    for (int i = 0; i < var_cnt; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            vars[i].val = val;
            return;
        }
    }
    if (var_cnt < 64) {
        strcpy(vars[var_cnt].name, name);
        vars[var_cnt].val = val;
        var_cnt++;
    }
}

static int eval_expr();

static int eval_primary() {
    if (tokens[tok_idx].type == TOK_NUMBER) {
        int v = tokens[tok_idx].val;
        tok_idx++;
        return v;
    }
    if (tokens[tok_idx].type == TOK_IDENT) {
        int v = get_var(tokens[tok_idx].text);
        tok_idx++;
        return v;
    }
    if (tokens[tok_idx].type == TOK_LPAREN) {
        tok_idx++;
        int v = eval_expr();
        if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
        return v;
    }
    return 0;
}

static int eval_expr() {
    int left = eval_primary();
    while (tokens[tok_idx].type == TOK_PLUS || tokens[tok_idx].type == TOK_MINUS ||
           tokens[tok_idx].type == TOK_STAR || tokens[tok_idx].type == TOK_SLASH ||
           tokens[tok_idx].type == TOK_EQ || tokens[tok_idx].type == TOK_LT ||
           tokens[tok_idx].type == TOK_GT) {
        token_type_t op = tokens[tok_idx].type;
        tok_idx++;
        int right = eval_primary();
        if (op == TOK_PLUS) left += right;
        else if (op == TOK_MINUS) left -= right;
        else if (op == TOK_STAR) left *= right;
        else if (op == TOK_SLASH) left = (right != 0) ? (left / right) : 0;
        else if (op == TOK_EQ) left = (left == right);
        else if (op == TOK_LT) left = (left < right);
        else if (op == TOK_GT) left = (left > right);
    }
    return left;
}

int run_unr_file(const char *unr_path, void (*print_fn)(const char *str, uint32_t color)) {
    if (fat32_open(unr_path) != 0) {
        if (print_fn) print_fn(".unr execution error: File not found", 0x00BF616A);
        return -1;
    }

    char magic[4];
    fat32_read(magic, 4);

    if (strncmp(magic, UNR_MAGIC, 4) != 0) {
        fat32_close();
        if (print_fn) print_fn("Invalid .unr format header!", 0x00BF616A);
        return -1;
    }

    fat32_read(&token_count, sizeof(int));
    fat32_read(tokens, token_count * sizeof(token_t));
    fat32_close();

    var_cnt = 0;
    tok_idx = 0;

    if (print_fn) print_fn("[ Executing UNR Executable ]", 0x00A3BE8C);

    while (tok_idx < token_count && tokens[tok_idx].type != TOK_EOF) {
        token_t *t = &tokens[tok_idx];

        if (t->type == TOK_INT) {
            tok_idx++;
            if (tokens[tok_idx].type == TOK_IDENT) {
                char varname[32];
                strcpy(varname, tokens[tok_idx].text);
                tok_idx++;
                int init_val = 0;
                if (tokens[tok_idx].type == TOK_ASSIGN) {
                    tok_idx++;
                    init_val = eval_expr();
                }
                set_var(varname, init_val);
                if (tokens[tok_idx].type == TOK_SEMICOLON) tok_idx++;
            }
            continue;
        }

        if (t->type == TOK_IDENT) {
            char varname[32];
            strcpy(varname, t->text);
            tok_idx++;
            if (tokens[tok_idx].type == TOK_ASSIGN) {
                tok_idx++;
                int val = eval_expr();
                set_var(varname, val);
                if (tokens[tok_idx].type == TOK_SEMICOLON) tok_idx++;
            }
            continue;
        }

        if (t->type == TOK_PRINTF) {
            tok_idx++;
            if (tokens[tok_idx].type == TOK_LPAREN) {
                tok_idx++;
                if (tokens[tok_idx].type == TOK_STRING) {
                    char fmt[128];
                    strcpy(fmt, tokens[tok_idx].text);
                    tok_idx++;

                    int arg_val = 0;
                    int has_arg = 0;
                    if (tokens[tok_idx].type == TOK_SEMICOLON || tokens[tok_idx].text[0] == ',') {
                        if (tokens[tok_idx].text[0] == ',') {
                            tok_idx++;
                            arg_val = eval_expr();
                            has_arg = 1;
                        }
                    }

                    if (tokens[tok_idx].type == TOK_RPAREN) tok_idx++;
                    if (tokens[tok_idx].type == TOK_SEMICOLON) tok_idx++;

                    char out_msg[256];
                    int m = 0;
                    for (int k = 0; fmt[k] != '\0'; k++) {
                        if (fmt[k] == '%' && fmt[k+1] == 'd' && has_arg) {
                            char nbuf[16];
                            int_to_str(arg_val, nbuf);
                            for (int nb = 0; nbuf[nb] != '\0'; nb++) out_msg[m++] = nbuf[nb];
                            k++;
                        } else {
                            out_msg[m++] = fmt[k];
                        }
                    }
                    out_msg[m] = '\0';
                    if (print_fn) print_fn(out_msg, 0x00ECEFF4);
                }
            }
            continue;
        }

        tok_idx++;
    }

    if (print_fn) print_fn("[ Process exited with code 0 ]", 0x0081A1C1);
    return 0;
}
