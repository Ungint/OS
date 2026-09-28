// Kernel/SourceI/canvas.c
#include "../Include/canvas.h"
#include "../Include/window.h"
#include "../Include/gfx.h"
#include "../Include/memory.h"
#include "../Include/string.h"

typedef struct
{
    int bound; // 1 nếu slot này đang được 1 cửa sổ dùng

    int width;
    int height;

    uint32_t *pixels; // width*height, cấp phát bằng malloc() (memory.c)

} canvas_t;

static canvas_t g_canvases[PROCESS_MAX];

static canvas_t *canvas_get(int id)
{
    if(id < 0 || id >= PROCESS_MAX)
        return 0;

    if(!g_canvases[id].bound)
        return 0;

    return &g_canvases[id];
}

int canvas_bind(process_t *win)
{
    canvas_t *c;
    int content_w;
    int content_h;
    uint32_t *pixels;
    uint32_t bytes;
    int i;

    if(!win)
        return CANVAS_INVALID_ID;

    if(win->id < 0 || win->id >= PROCESS_MAX)
        return CANVAS_INVALID_ID;

    content_w = win->width  - WINDOW_BORDER * 2;
    content_h = win->height - WINDOW_TITLEBAR_HEIGHT - WINDOW_BORDER;

    if(content_w < 1) content_w = 1;
    if(content_h < 1) content_h = 1;

    bytes = (uint32_t)content_w * (uint32_t)content_h * 4u;

    pixels = (uint32_t *)malloc(bytes);

    if(!pixels)
        return CANVAS_INVALID_ID;

    c = &g_canvases[win->id];

    c->width  = content_w;
    c->height = content_h;
    c->pixels = pixels;

    for(i = 0; i < content_w * content_h; i++)
        c->pixels[i] = 0x00000000;

    c->bound = 1;

    return win->id;
}

void canvas_unbind(int id)
{
    canvas_t *c = canvas_get(id);

    if(!c)
        return;

    c->bound  = 0;
    c->pixels = 0;
}

int canvas_width(int id)
{
    canvas_t *c = canvas_get(id);
    return c ? c->width : 0;
}

int canvas_height(int id)
{
    canvas_t *c = canvas_get(id);
    return c ? c->height : 0;
}

void canvas_clear(int id, uint32_t color)
{
    canvas_t *c = canvas_get(id);
    int i;
    int n;

    if(!c)
        return;

    n = c->width * c->height;

    for(i = 0; i < n; i++)
        c->pixels[i] = color;
}

void canvas_put_pixel(int id, int x, int y, uint32_t color)
{
    canvas_t *c = canvas_get(id);

    if(!c)
        return;

    if(x < 0 || x >= c->width || y < 0 || y >= c->height)
        return;

    c->pixels[y * c->width + x] = color;
}

uint32_t canvas_get_pixel(int id, int x, int y)
{
    canvas_t *c = canvas_get(id);

    if(!c)
        return 0;

    if(x < 0 || x >= c->width || y < 0 || y >= c->height)
        return 0;

    return c->pixels[y * c->width + x];
}

void canvas_fill_rect(int id, int x, int y, int w, int h, uint32_t color)
{
    canvas_t *c = canvas_get(id);
    int ex;
    int ey;
    int px;
    int py;

    if(!c)
        return;

    if(w < 0 || h < 0)
        return;

    ex = x + w;
    ey = y + h;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(ex > c->width)  ex = c->width;
    if(ey > c->height) ey = c->height;

    for(py = y; py < ey; py++)
        for(px = x; px < ex; px++)
            c->pixels[py * c->width + px] = color;
}

void canvas_draw_rect(int id, int x, int y, int w, int h, uint32_t color)
{
    if(w <= 0 || h <= 0)
        return;

    canvas_fill_rect(id, x,         y,         w, 1, color);
    canvas_fill_rect(id, x,         y + h - 1, w, 1, color);
    canvas_fill_rect(id, x,         y,         1, h, color);
    canvas_fill_rect(id, x + w - 1, y,         1, h, color);
}

void canvas_draw_line(int id, int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int e2;

    if(!canvas_get(id))
        return;

    dx = x1 - x0; if(dx < 0) dx = -dx;
    dy = y1 - y0; if(dy < 0) dy = -dy;

    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;

    err = dx - dy;

    while(1)
    {
        canvas_put_pixel(id, x0, y0, color);

        if(x0 == x1 && y0 == y1)
            break;

        e2 = 2 * err;

        if(e2 > -dy)
        {
            err -= dy;
            x0  += sx;
        }

        if(e2 < dx)
        {
            err += dx;
            y0  += sy;
        }
    }
}

void canvas_draw_circle(int id, int cx, int cy, int r, uint32_t color)
{
    int x;
    int y;
    int d;

    if(!canvas_get(id) || r < 0)
        return;

    x = r;
    y = 0;
    d = 1 - r;

    while(x >= y)
    {
        canvas_put_pixel(id, cx + x, cy + y, color);
        canvas_put_pixel(id, cx + y, cy + x, color);
        canvas_put_pixel(id, cx - y, cy + x, color);
        canvas_put_pixel(id, cx - x, cy + y, color);
        canvas_put_pixel(id, cx - x, cy - y, color);
        canvas_put_pixel(id, cx - y, cy - x, color);
        canvas_put_pixel(id, cx + y, cy - x, color);
        canvas_put_pixel(id, cx + x, cy - y, color);

        y++;

        if(d <= 0)
        {
            d += 2 * y + 1;
        }
        else
        {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void canvas_fill_circle(int id, int cx, int cy, int r, uint32_t color)
{
    int x;
    int y;
    int d;
    int i;

    if(!canvas_get(id) || r < 0)
        return;

    x = r;
    y = 0;
    d = 1 - r;

    while(x >= y)
    {
        for(i = cx - x; i <= cx + x; i++)
        {
            canvas_put_pixel(id, i, cy + y, color);
            canvas_put_pixel(id, i, cy - y, color);
        }

        for(i = cx - y; i <= cx + y; i++)
        {
            canvas_put_pixel(id, i, cy + x, color);
            canvas_put_pixel(id, i, cy - x, color);
        }

        y++;

        if(d <= 0)
        {
            d += 2 * y + 1;
        }
        else
        {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void canvas_draw(process_t *p)
{
    canvas_t *c;
    int content_x;
    int content_y;
    int y;

    window_draw_process(p);

    if(!p)
        return;

    c = canvas_get(p->id);

    if(!c)
        return;

    content_x = p->x + WINDOW_BORDER;
    content_y = p->y + WINDOW_TITLEBAR_HEIGHT;

    uint32_t *fb = gfx_get_backbuffer();
    uint32_t fb_w = gfx_width();
    uint32_t fb_h = gfx_height();

    if(!fb || fb_w == 0 || fb_h == 0)
        return;

    for(y = 0; y < c->height; y++)
    {
        int screen_y = content_y + y;
        if(screen_y < 0 || screen_y >= (int)fb_h)
            continue;

        int draw_w = c->width;
        if(content_x < 0 || content_x >= (int)fb_w)
            continue;

        if(content_x + draw_w > (int)fb_w)
            draw_w = (int)fb_w - content_x;

        if(draw_w > 0)
        {
            uint32_t *dst = &fb[screen_y * fb_w + content_x];
            const uint32_t *src = &c->pixels[y * c->width];
            memcpy(dst, src, (uint32_t)draw_w * 4u);
        }
    }
}
