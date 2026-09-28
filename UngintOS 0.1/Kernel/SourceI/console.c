// Kernel/SourceI/console.c
#include "../Include/console.h"
#include "../Include/window.h"
#include "../Include/gfx.h"
#include "../Include/font.h"
#include "../Include/string.h"

#include <stdarg.h>

typedef struct
{
    int bound; // 1 nếu slot này đang được 1 cửa sổ dùng

    int cols;
    int rows;

    int cur_row;
    int cur_col;

    uint32_t fg;

    char     buf[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS + 1];
    uint32_t line_color[CONSOLE_MAX_ROWS];

} console_t;

// Mỗi process id (0..PROCESS_MAX-1) có 1 console riêng -> mở nhiều
// console/cửa sổ cùng lúc sẽ không còn ghi đè lên nhau nữa.
static console_t g_consoles[PROCESS_MAX];

// ============================================
// NỘI BỘ
// ============================================

static console_t *console_get(int id)
{
    if(id < 0 || id >= PROCESS_MAX)
        return 0;

    if(!g_consoles[id].bound)
        return 0;

    return &g_consoles[id];
}

static void console_clear_row(console_t *c, int row)
{
    int i;

    for(i = 0; i <= CONSOLE_MAX_COLS; i++)
        c->buf[row][i] = 0;

    c->line_color[row] = c->fg;
}

static void console_scroll(console_t *c)
{
    int r;

    for(r = 0; r < c->rows - 1; r++)
    {
        strcpy(c->buf[r], c->buf[r + 1]);
        c->line_color[r] = c->line_color[r + 1];
    }

    console_clear_row(c, c->rows - 1);
}

static void console_newline(console_t *c)
{
    c->cur_col = 0;
    c->cur_row++;

    if(c->cur_row >= c->rows)
    {
        console_scroll(c);
        c->cur_row = c->rows - 1;
    }

    console_clear_row(c, c->cur_row);
}

static void console_do_putc(console_t *c, char ch)
{
    if(c->rows <= 0 || c->cols <= 0)
        return;

    if(ch == '\n')
    {
        console_newline(c);
        return;
    }

    if(ch == '\r')
    {
        c->cur_col = 0;
        return;
    }

    if(ch == '\b')
    {
        if(c->cur_col > 0)
        {
            c->cur_col--;
            c->buf[c->cur_row][c->cur_col] = 0;
        }
        return;
    }

    if(ch == '\t')
    {
        int i;
        for(i = 0; i < 4; i++)
            console_do_putc(c, ' ');
        return;
    }

    if(c->cur_col == 0)
        c->line_color[c->cur_row] = c->fg;

    if(c->cur_col >= c->cols)
        console_newline(c);

    c->buf[c->cur_row][c->cur_col] = ch;
    c->buf[c->cur_row][c->cur_col + 1] = 0;
    c->cur_col++;
}

static void console_do_write(console_t *c, const char *str)
{
    if(!str)
        return;

    while(*str)
        console_do_putc(c, *str++);
}

// ---- printf tối giản, không dùng libc ----

static void console_write_uint(console_t *c, uint32_t value, int base, int upper)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = upper ? digits_upper : digits_lower;

    char tmp[32];
    int i = 0;

    if(value == 0)
    {
        console_do_putc(c, '0');
        return;
    }

    while(value > 0 && i < (int)sizeof(tmp))
    {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }

    while(i > 0)
        console_do_putc(c, tmp[--i]);
}

static void console_write_int(console_t *c, int value, int base)
{
    if(value < 0)
    {
        console_do_putc(c, '-');
        console_write_uint(c, (uint32_t)(-value), base, 0);
    }
    else
    {
        console_write_uint(c, (uint32_t)value, base, 0);
    }
}

static void console_vprintf(console_t *c, const char *fmt, va_list ap)
{
    while(*fmt)
    {
        char ch = *fmt++;

        if(ch != '%')
        {
            console_do_putc(c, ch);
            continue;
        }

        ch = *fmt++;

        switch(ch)
        {
            case 'd':
            case 'i':
                console_write_int(c, va_arg(ap, int), 10);
                break;

            case 'u':
                console_write_uint(c, va_arg(ap, unsigned int), 10, 0);
                break;

            case 'x':
                console_write_uint(c, va_arg(ap, unsigned int), 16, 0);
                break;

            case 'X':
                console_write_uint(c, va_arg(ap, unsigned int), 16, 1);
                break;

            case 'p':
                console_do_write(c, "0x");
                console_write_uint(c, (uint32_t)(uintptr_t)va_arg(ap, void *), 16, 0);
                break;

            case 'c':
                console_do_putc(c, (char)va_arg(ap, int));
                break;

            case 's':
            {
                const char *s = va_arg(ap, const char *);
                console_do_write(c, s ? s : "(null)");
                break;
            }

            case '%':
                console_do_putc(c, '%');
                break;

            case 0:
                return;

            default:
                console_do_putc(c, '%');
                console_do_putc(c, ch);
                break;
        }
    }
}

// ============================================
// API
// ============================================

int console_bind(process_t *win)
{
    console_t *c;
    int content_w;
    int content_h;
    int gw;
    int gh;
    int r;

    if(!win)
        return CONSOLE_INVALID_ID;

    if(win->id < 0 || win->id >= PROCESS_MAX)
        return CONSOLE_INVALID_ID;

    c = &g_consoles[win->id];

    gw = font_glyph_w();
    gh = font_glyph_h();

    if(gw <= 0) gw = 8;
    if(gh <= 0) gh = 16;

    content_w = win->width  - WINDOW_BORDER * 2 - CONSOLE_PADDING * 2;
    content_h = win->height - WINDOW_TITLEBAR_HEIGHT - WINDOW_BORDER - CONSOLE_PADDING * 2;

    if(content_w < 0) content_w = 0;
    if(content_h < 0) content_h = 0;

    c->cols = content_w / gw;
    c->rows = content_h / gh;

    if(c->cols > CONSOLE_MAX_COLS) c->cols = CONSOLE_MAX_COLS;
    if(c->rows > CONSOLE_MAX_ROWS) c->rows = CONSOLE_MAX_ROWS;

    if(c->cols < 1) c->cols = 1;
    if(c->rows < 1) c->rows = 1;

    c->cur_row = 0;
    c->cur_col = 0;
    c->fg      = 0x00FFFFFF;

    for(r = 0; r < c->rows; r++)
        console_clear_row(c, r);

    c->bound = 1;

    return win->id;
}

void console_unbind(int id)
{
    console_t *c = console_get(id);

    if(!c)
        return;

    c->bound = 0;
}

void console_clear(int id)
{
    console_t *c = console_get(id);
    int r;

    if(!c)
        return;

    c->cur_row = 0;
    c->cur_col = 0;

    for(r = 0; r < c->rows; r++)
        console_clear_row(c, r);
}

void console_set_color(int id, uint32_t fg)
{
    console_t *c = console_get(id);

    if(!c)
        return;

    c->fg = fg;
}

void console_putc(int id, char ch)
{
    console_t *c = console_get(id);

    if(!c)
        return;

    console_do_putc(c, ch);
}

void console_write(int id, const char *str)
{
    console_t *c = console_get(id);

    if(!c)
        return;

    console_do_write(c, str);
}

void console_printf(int id, const char *fmt, ...)
{
    console_t *c = console_get(id);
    va_list ap;

    if(!c)
        return;

    va_start(ap, fmt);
    console_vprintf(c, fmt, ap);
    va_end(ap);
}

void console_printf_color(int id, uint32_t fg, const char *fmt, ...)
{
    console_t *c = console_get(id);
    va_list ap;
    uint32_t old;

    if(!c)
        return;

    old   = c->fg;
    c->fg = fg;

    if(c->cur_col == 0)
        c->line_color[c->cur_row] = fg;

    va_start(ap, fmt);
    console_vprintf(c, fmt, ap);
    va_end(ap);

    c->fg = old;
}

void console_draw(process_t *p)
{
    console_t *c;
    int content_x;
    int content_y;
    int gh;
    int r;

    window_draw_process(p);

    if(!p)
        return;

    c = console_get(p->id);

    if(!c)
        return;

    gh = font_glyph_h();
    if(gh <= 0) gh = 16;

    content_x = p->x + WINDOW_BORDER + CONSOLE_PADDING;
    content_y = p->y + WINDOW_TITLEBAR_HEIGHT + CONSOLE_PADDING;

    for(r = 0; r < c->rows; r++)
    {
        if(c->buf[r][0] == 0)
            continue;

        font_draw_string(
            (uint32_t)content_x,
            (uint32_t)(content_y + r * gh),
            c->buf[r],
            c->line_color[r],
            GFX_TRANSPARENT
        );
    }
}
