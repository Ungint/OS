#include "../Include/fbcon.h"
#include "../Include/fbfont.h"
#include "../Include/font.h"

static void fbcon_pixel(
    fbcon_t *con,
    uint32_t x,
    uint32_t y,
    uint32_t color
)
{
    if(!con || !con->buffer)
        return;

    if(x >= con->width || y >= con->height)
        return;

    con->buffer[y * con->pitch + x] = color;
}

static void fbcon_clear_char(
    fbcon_t *con,
    uint32_t x,
    uint32_t y
)
{
    if(!con)
        return;

    uint32_t px = x * font_glyph_w();
    uint32_t py = y * font_glyph_h();

    for(uint32_t yy = 0; yy < font_glyph_h(); yy++)
    {
        for(uint32_t xx = 0; xx < font_glyph_w(); xx++)
        {
            fbcon_pixel(
                con,
                px + xx,
                py + yy,
                con->bg
            );
        }
    }
}

int fbcon_init(
    fbcon_t *con,
    uint32_t *buffer,
    uint32_t width,
    uint32_t height,
    uint32_t pitch
)
{
    if(!con || !buffer)
        return 0;

    con->buffer = buffer;

    con->width = width;
    con->height = height;

    /*
     * fbcon lưu pitch theo số pixel.
     * pitch truyền vào là byte.
     */
    con->pitch = pitch / 4;

    con->fg = 0xFFFFFFFF;
    con->bg = 0x00000000;

    con->cursor_x = 0;
    con->cursor_y = 0;

    con->visible = 1;

    fbcon_clear(con);

    return 1;
}

void fbcon_clear(fbcon_t *con)
{
    if(!con || !con->buffer)
        return;

    for(uint32_t y = 0; y < con->height; y++)
    {
        for(uint32_t x = 0; x < con->width; x++)
        {
            con->buffer[y * con->pitch + x] = con->bg;
        }
    }

    con->cursor_x = 0;
    con->cursor_y = 0;
}

void fbcon_set_color(
    fbcon_t *con,
    uint32_t fg,
    uint32_t bg
)
{
    if(!con)
        return;

    con->fg = fg;
    con->bg = bg;
}

void fbcon_scroll(fbcon_t *con)
{
    if(!con || !con->buffer)
        return;

    uint32_t char_height = font_glyph_h();

    if(char_height == 0)
        return;

    if(char_height >= con->height)
    {
        fbcon_clear(con);
        return;
    }

    for(uint32_t y = 0; y < con->height - char_height; y++)
    {
        uint32_t *dst =
            &con->buffer[y * con->pitch];

        uint32_t *src =
            &con->buffer[(y + char_height) * con->pitch];

        for(uint32_t x = 0; x < con->width; x++)
            dst[x] = src[x];
    }

    for(uint32_t y = con->height - char_height;
        y < con->height;
        y++)
    {
        for(uint32_t x = 0; x < con->width; x++)
        {
            con->buffer[y * con->pitch + x] = con->bg;
        }
    }

    if(con->cursor_y > 0)
        con->cursor_y--;
}

void fbcon_newline(fbcon_t *con)
{
    if(!con)
        return;

    uint32_t char_height = font_glyph_h();

    if(char_height == 0)
        return;

    con->cursor_x = 0;
    con->cursor_y++;

    uint32_t chars_y =
        con->height / char_height;

    if(chars_y == 0)
        return;

    if(con->cursor_y >= chars_y)
    {
        fbcon_scroll(con);

        con->cursor_y = chars_y - 1;
    }
}

void fbcon_backspace(fbcon_t *con)
{
    if(!con)
        return;

    if(con->cursor_x > 0)
    {
        con->cursor_x--;

        fbcon_clear_char(
            con,
            con->cursor_x,
            con->cursor_y
        );
    }
}

void fbcon_putchar(
    fbcon_t *con,
    char c
)
{
    if(!con)
        return;

    uint32_t char_width = font_glyph_w();
    uint32_t char_height = font_glyph_h();

    if(char_width == 0 || char_height == 0)
        return;

    if(c == '\n')
    {
        fbcon_newline(con);
        return;
    }

    if(c == '\r')
    {
        con->cursor_x = 0;
        return;
    }

    if(c == '\b')
    {
        fbcon_backspace(con);
        return;
    }

    uint32_t chars_x =
        con->width / char_width;

    uint32_t chars_y =
        con->height / char_height;

    if(chars_x == 0 || chars_y == 0)
        return;

    if(con->cursor_x >= chars_x)
        fbcon_newline(con);

    if(con->cursor_y >= chars_y)
    {
        fbcon_scroll(con);
        con->cursor_y = chars_y - 1;
    }

    uint32_t px =
        con->cursor_x * char_width;

    uint32_t py =
        con->cursor_y * char_height;

    /*
     * fbfont pitch dùng BYTE.
     * fbcon pitch đang lưu PIXEL.
     */
    fbfont_draw_char(
        con->buffer,
        con->width,
        con->height,
        con->pitch * 4,
        px,
        py,
        c,
        con->fg,
        con->bg
    );

    con->cursor_x++;
}

void fbcon_print(
    fbcon_t *con,
    const char *str
)
{
    if(!con || !str)
        return;

    while(*str)
    {
        fbcon_putchar(con, *str);
        str++;
    }
}

void fbcon_set_id(
    fbcon_t *con,
    uint32_t id
)
{
    if(!con)
        return;

    con->id = id;
}

/*
 * In một số uint32_t ra console (không dùng string.h/itoa
 * để module này tự đứng độc lập được).
 */
static void fbcon_print_uint(
    fbcon_t *con,
    uint32_t value
)
{
    char buf[10];
    int i = 0;

    if(value == 0)
    {
        fbcon_putchar(con, '0');
        return;
    }

    while(value > 0 && i < (int)sizeof(buf))
    {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while(i > 0)
    {
        i--;
        fbcon_putchar(con, buf[i]);
    }
}

/*
 * In ra console kèm id, dạng: "[id] text"
 */
void fbcon_printid(
    fbcon_t *con,
    uint32_t id,
    const char *str
)
{
    if(!con || !str)
        return;

    con->id = id;

    fbcon_putchar(con, '[');
    fbcon_print_uint(con, id);
    fbcon_putchar(con, ']');
    fbcon_putchar(con, ' ');

    fbcon_print(con, str);
}