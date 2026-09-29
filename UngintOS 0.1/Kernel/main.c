#include "../Include/stdint.h"
#include "../Include/boot_data.h"
#include "../Include/memory.h"
#include "../Include/gfx.h"
#include "../Include/font.h"
#include "../Include/keyboard.h"
#include "../Include/mouse.h"
#include "../Include/timer.h"
#include "../Include/fat32.h"
#include "../Include/image.h"
#include "../Include/process.h"
#include "../Include/window.h"
#include "../Include/canvas.h"
#include "../Include/wm.h"
#include "../Include/explorer.h"
#include "../Include/editor.h"
#include "../Include/terminal.h"

#define GFXWIN_X 0
#define GFXWIN_Y 0
#define GFXWIN_W 1280
#define GFXWIN_H 720
#define MOUSE_UPDATE_MS 25

static int gfxwin_id = CANVAS_INVALID_ID;
static IMAGE desktop;
static IMAGE mouse;

static void loading_draw(uint32_t percent,const char *status)
{
    uint32_t w = gfx_width();
    uint32_t h = gfx_height();
    uint32_t bar_w = 500;
    uint32_t bar_h = 18;
    uint32_t bar_x = (w - bar_w) / 2;
    uint32_t bar_y = h / 2 + 20;
    uint32_t fill_w = (bar_w * percent) / 100;
    char text[8];
    int n = 0;

    gfx_clear(0x00101018);

    font_draw_string(
        (int)(w / 2 - 100),
        (int)(h / 2 - 100),
        "System is Loading...",
        0x00FFFFFF,
        GFX_TRANSPARENT
    );

    gfx_fill_rect(
        bar_x,
        bar_y,
        bar_w,
        bar_h,
        0x00303040
    );

    if(fill_w)
        gfx_fill_rect(
            bar_x,
            bar_y,
            fill_w,
            bar_h,
            0x0044AAFF
        );

    text[n++] = '0' + (percent / 100) % 10;
    text[n++] = '0' + (percent / 10) % 10;
    text[n++] = '0' + percent % 10;
    text[n++] = '%';
    text[n] = '\0';

    font_draw_string(
        (int)(w / 2 - 15),
        bar_y + 30,
        text,
        0x00FFFFFF,
        GFX_TRANSPARENT
    );

    font_draw_string(
        (int)(w / 2 - 120),
        bar_y + 60,
        status,
        0x00AAAAAA,
        GFX_TRANSPARENT
    );

    gfx_present();
}

static void gfxwin_draw(process_t *p)
{
    if(gfxwin_id != CANVAS_INVALID_ID)
        explorer_draw(gfxwin_id);

    canvas_draw(p);
}

static void gfxwin_init(void)
{
    process_t *win;

    gfxwin_id = run_process(
        PROCESS_GRAPHICS_WINDOW,
        "UngintOS File Explorer",
        GFXWIN_X,
        GFXWIN_Y,
        GFXWIN_W,
        GFXWIN_H,
        0,
        gfxwin_draw,
        0
    );

    if(gfxwin_id < 0)
        return;

    win = process_get(gfxwin_id);

    if(!win)
    {
        gfxwin_id = CANVAS_INVALID_ID;
        return;
    }

    gfxwin_id = canvas_bind(win);

    if(gfxwin_id == CANVAS_INVALID_ID)
        return;

    explorer_init(gfxwin_id);
}

static void desktop_draw(void)
{
    if(desktop.loaded)
        useimage(&desktop,0,0);
}

static void draw_mouse(void)
{
    if(mouse.loaded)
        useimage(
            &mouse,
            (uint32_t)mouse_x(),
            (uint32_t)mouse_y()
        );
}

static const char *image_error_name(image_error_t error)
{
    switch(error)
    {
        case IMG_OK: return "IMG_OK";
        case IMG_ERR_OPEN: return "IMG_ERR_OPEN";
        case IMG_ERR_READ: return "IMG_ERR_READ";
        case IMG_ERR_INVALID: return "IMG_ERR_INVALID";
        case IMG_ERR_UNSUPPORTED: return "IMG_ERR_UNSUPPORTED";
        case IMG_ERR_MEMORY: return "IMG_ERR_MEMORY";
        case IMG_ERR_UNKNOWN_FORMAT: return "IMG_ERR_UNKNOWN_FORMAT";
        default: return "IMG_ERR_UNKNOWN";
    }
}

static void fatal_error(
    const char *title,
    const char *message
)
{
    gfx_clear(0x00101018);

    font_draw_string(
        100,
        200,
        "UNGINTOS FATAL ERROR",
        0x00FF4444,
        GFX_TRANSPARENT
    );

    font_draw_string(
        100,
        240,
        title,
        0x00FFFFFF,
        GFX_TRANSPARENT
    );

    font_draw_string(
        100,
        280,
        message,
        0x00FFAA00,
        GFX_TRANSPARENT
    );

    gfx_present();

    while(1)
    {
        keyboard_poll();
        mouse_poll();
    }
}

__attribute__((section(".text._start")))
void _start(void)
{
    boot_data_load();

    if(!vbe_ok)
        return;

    memory_init();

    if(!gfx_init_backbuffer())
        return;

    font_init();

    loading_draw(10,"Initializing keyboard...");
    keyboard_init();

    loading_draw(20,"Initializing mouse...");
    mouse_init(
        boot_width,
        boot_height
    );

    loading_draw(30,"Initializing timer...");
    timer_init();

    loading_draw(40,"Starting process manager...");
    process_manager_init();
    editor_init_all();
    terminal_init_all();

    loading_draw(50,"Initializing filesystem...");

    if(fat32_init(0x81) != 0)
    {
        fatal_error(
            "FAT32 initialization failed!",
            "Cannot initialize the filesystem."
        );
    }

    loading_draw(70,"Loading desktop image...");

    desktop = loadimage(
        "/.OS/Image/DesktopMain.png"
    );

    if(!desktop.loaded)
    {
        fatal_error(
            "Failed to load desktop image!",
            image_error_name(
                loadimage_last_error()
            )
        );
    }

    loading_draw(80,"Loading mouse image...");

    mouse = loadimage(
        "/.OS/Image/Mouse.png"
    );

    if(!mouse.loaded)
    {
        freeimage(&desktop);

        fatal_error(
            "Failed to load mouse image!",
            image_error_name(
                loadimage_last_error()
            )
        );
    }

    loading_draw(90,"Starting window manager...");

    gfx_layer_init();
    gfxwin_init();

    loading_draw(100,"System ready!");

    for(volatile uint64_t i = 0;i < 30000000;i++);

    int prev_mx = -100000;
    int prev_my = -100000;
    int prev_click = 0;
    int force_full = 1;

    uint64_t last_full_ms = timer_ms();
    uint64_t last_mouse_update = timer_ms();

    uint64_t fps_time = timer_ms();
    uint32_t frames = 0;
    uint32_t fps = 0;

    uint32_t mouse_w = mouse.loaded ? mouse.width : 0;
    uint32_t mouse_h = mouse.loaded ? mouse.height : 0;

    while(1)
    {
        int mx;
        int my;
        int click;
        int need_full;
        int presented = 0;
        uint64_t now;

        keyboard_poll();
        mouse_poll();

        mx = mouse_x();
        my = mouse_y();
        click = mouse_left_button();

        int click_rising_edge = (click && !prev_click);

        wm_handle_mouse(
            mx,
            my,
            click
        );

        if(click_rising_edge)
        {
            int top_p_id = -1;
            int top_layer = 1000;

            for(int i = 0; i < PROCESS_MAX; i++)
            {
                process_t *p = process_get(i);
                if (!p || !p->visible) continue;
                if (mx >= p->x && mx < p->x + p->width &&
                    my >= p->y && my < p->y + p->height)
                {
                    if (p->layer < top_layer)
                    {
                        top_layer = p->layer;
                        top_p_id = p->id;
                    }
                }
            }

            if(top_p_id != -1)
            {
                process_t *p = process_get(top_p_id);
                if(p)
                {
                    int close_x = p->x + p->width - 25;
                    int close_y = p->y + 4;
                    if(mx >= close_x && mx <= close_x + 20 &&
                        my >= close_y && my <= close_y + 20)
                    {
                        editor_close_by_id(p->id);
                        terminal_close_by_id(p->id);
                        close_process(p->id);
                        if(p->id == gfxwin_id)
                        {
                            gfxwin_id = CANVAS_INVALID_ID;
                        }
                    }
                    else
                    {
                        int rel_x = mx - (p->x + WINDOW_BORDER);
                        int rel_y = my - (p->y + WINDOW_TITLEBAR_HEIGHT);

                        if(rel_x >= 0 && rel_y >= 0)
                        {
                            if(p->id == gfxwin_id)
                            {
                                explorer_handle_click(p->id, rel_x, rel_y, click);
                            }
                            else
                            {
                                editor_handle_click(p->id, rel_x, rel_y, click);
                                terminal_handle_click(p->id, rel_x, rel_y, click);
                            }
                        }
                    }
                }
            }
        }

        key_event_t key_ev;
        while (get_key_event(&key_ev))
        {
            for(int i = 0; i < PROCESS_MAX; i++)
            {
                process_t *p = process_get(i);
                if (p && p->focused && p->id != gfxwin_id)
                {
                    editor_handle_key(p->id, key_ev.ascii, key_ev.scancode);
                    terminal_handle_key(p->id, key_ev.ascii, key_ev.scancode);
                    break;
                }
            }
        }

        now = timer_ms();

        need_full =
            force_full ||
            click != prev_click ||
            now - last_full_ms >= 250;

        if(need_full)
        {
            wm_composite(
                desktop_draw,
                0
            );

            gfx_layer_snapshot();

            if(mouse.loaded)
            {
                gfx_composite_image(
                    (uint32_t)mx,
                    (uint32_t)my,
                    mouse_w,
                    mouse_h,
                    mouse.pixels,
                    mouse_w,
                    mouse_h
                );
            }

            gfx_present();
            presented = 1;

            force_full = 0;
            last_full_ms = now;

            prev_mx = mx;
            prev_my = my;
            last_mouse_update = now;
        }
        else
        {
            if((mx != prev_mx || my != prev_my) &&
               now - last_mouse_update >= MOUSE_UPDATE_MS)
            {
                gfx_layer_restore_rect(
                    (uint32_t)prev_mx,
                    (uint32_t)prev_my,
                    mouse_w,
                    mouse_h
                );

                if(mouse.loaded)
                {
                    gfx_composite_image(
                        (uint32_t)mx,
                        (uint32_t)my,
                        mouse_w,
                        mouse_h,
                        mouse.pixels,
                        mouse_w,
                        mouse_h
                    );
                }

                gfx_present_rect(
                    (uint32_t)prev_mx,
                    (uint32_t)prev_my,
                    mouse_w,
                    mouse_h
                );

                gfx_present_rect(
                    (uint32_t)mx,
                    (uint32_t)my,
                    mouse_w,
                    mouse_h
                );

                presented = 1;

                prev_mx = mx;
                prev_my = my;
                last_mouse_update = now;
            }
        }

        prev_click = click;

        if(presented)
            frames++;

        now = timer_ms();

        if(now - fps_time >= 1000)
        {
            fps = frames;
            explorer_set_fps(fps);

            frames = 0;
            fps_time = now;
        }
    }
}