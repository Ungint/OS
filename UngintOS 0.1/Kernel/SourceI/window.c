#include "../Include/window.h"
#include "../Include/gfx.h"
#include "../Include/font.h"

static void draw_title(
    process_t* p
)
{
    uint32_t title_color;

    if(p->focused)
        title_color=WINDOW_COLOR_TITLE_FOCUS;
    else
        title_color=WINDOW_COLOR_TITLE;

    gfx_fill_rect(
        p->x,
        p->y,
        p->width,
        WINDOW_TITLEBAR_HEIGHT,
        title_color
    );

    font_draw_string(
        p->x+8,
        p->y+8,
        p->title,
        0x00FFFFFF,
        GFX_TRANSPARENT
    );

    font_draw_string(
        p->x+p->width-20,
        p->y+8,
        "X",
        0x00FFFFFF,
        GFX_TRANSPARENT
    );
}

void window_draw_frame(
    process_t* p
)
{
    gfx_fill_rect(
        p->x,
        p->y,
        p->width,
        p->height,
        WINDOW_COLOR_BORDER
    );

    gfx_fill_rect(
        p->x+WINDOW_BORDER,
        p->y+WINDOW_BORDER,
        p->width-WINDOW_BORDER*2,
        p->height-WINDOW_BORDER*2,
        WINDOW_COLOR_BG
    );

    draw_title(p);
}

void window_draw_graphics(
    process_t* p
)
{
    window_draw_frame(p);
}

void window_draw_text(
    process_t* p
)
{
    window_draw_frame(p);
}

void window_draw_warning(
    process_t* p
)
{
    window_draw_frame(p);

    gfx_fill_rect(
        p->x+WINDOW_BORDER,
        p->y+WINDOW_TITLEBAR_HEIGHT,
        p->width-WINDOW_BORDER*2,
        p->height-WINDOW_TITLEBAR_HEIGHT-WINDOW_BORDER,
        0x00202010
    );
}

void window_draw_error(
    process_t* p
)
{
    window_draw_frame(p);

    gfx_fill_rect(
        p->x+WINDOW_BORDER,
        p->y+WINDOW_TITLEBAR_HEIGHT,
        p->width-WINDOW_BORDER*2,
        p->height-WINDOW_TITLEBAR_HEIGHT-WINDOW_BORDER,
        0x00201010
    );
}

void window_draw_process(
    process_t* p
)
{
    if(!p)
        return;

    switch(p->type)
    {
        case PROCESS_GRAPHICS_WINDOW:
            window_draw_graphics(p);
            break;

        case PROCESS_TEXT_WINDOW:
            window_draw_text(p);
            break;

        case PROCESS_WARNING_POPUP:
            window_draw_warning(p);
            break;

        case PROCESS_ERROR_POPUP:
            window_draw_error(p);
            break;
    }
}