#include "../Include/process.h"
#include "../Include/string.h"
#include "../Include/console.h"
#include "../Include/canvas.h"
#include "../Include/wm.h"

static process_t processes[PROCESS_MAX];

void process_manager_init(void)
{
    int i;

    for(i=0;i<PROCESS_MAX;i++)
    {
        processes[i].used=0;
        processes[i].id=i;

        processes[i].type=0;

        processes[i].x=0;
        processes[i].y=0;
        processes[i].width=0;
        processes[i].height=0;

        processes[i].visible=0;
        processes[i].focused=0;

        processes[i].layer=LAYER_DESKTOP; // sentinel: chưa dùng, không tham gia layer stack

        processes[i].title[0]=0;

        processes[i].update=0;
        processes[i].draw=0;
        processes[i].data=0;
    }
}

int run_process(
    int type,
    const char* title,
    int x,
    int y,
    int width,
    int height,
    process_update_t update,
    process_draw_t draw,
    void* data
)
{
    int i;

    if(
        type<PROCESS_GRAPHICS_WINDOW ||
        type>PROCESS_ERROR_POPUP
    )
        return -1;

    for(i=0;i<PROCESS_MAX;i++)
    {
        if(processes[i].used)
            continue;

        processes[i].used=1;
        processes[i].id=i;

        processes[i].type=type;

        processes[i].x=x;
        processes[i].y=y;

        processes[i].width=width;
        processes[i].height=height;

        processes[i].visible=1;
        processes[i].focused=0;

        processes[i].layer=LAYER_DESKTOP; // sentinel "chưa có layer" cho tới khi wm_bring_to_front() gán

        strcpy(
            processes[i].title,
            title
        );

        processes[i].update=update;
        processes[i].draw=draw;

        processes[i].data=data;

        // Cửa sổ mới mở luôn xuất hiện TRÊN CÙNG (layer 1), giống hệt
        // hành vi khi người dùng click chuột trái vào 1 cửa sổ.
        wm_bring_to_front(i);

        return i;
    }

    return -1;
}

void close_process(int id)
{
    if(id<0 || id>=PROCESS_MAX)
        return;

    if(!processes[id].used)
        return;

    // Dồn (compact) lại các layer phía sau, TRƯỚC khi xoá used/layer.
    wm_release_layer(id);

    processes[id].used=0;
    processes[id].visible=0;

    processes[id].update=0;
    processes[id].draw=0;
    processes[id].data=0;

    // Nếu id này có console hoặc canvas gắn vào, gỡ luôn để id có thể
    // được bind lại từ đầu khi 1 process khác tái sử dụng id này -
    // đảm bảo KHÔNG BAO GIỜ 2 cửa sổ dính chung 1 buffer console/canvas.
    console_unbind(id);
    canvas_unbind(id);
}

void process_update(void)
{
    int i;

    for(i=0;i<PROCESS_MAX;i++)
    {
        process_t* p=&processes[i];

        if(!p->used)
            continue;

        if(!p->visible)
            continue;

        if(p->update!=0)
            p->update(p);
    }
}

void process_draw(void)
{
    int i;

    for(i=0;i<PROCESS_MAX;i++)
    {
        process_t* p=&processes[i];

        if(!p->used)
            continue;

        if(!p->visible)
            continue;

        if(p->draw!=0)
            p->draw(p);
    }
}

process_t* process_get(int id)
{
    if(id<0 || id>=PROCESS_MAX)
        return 0;

    if(!processes[id].used)
        return 0;

    return &processes[id];
}