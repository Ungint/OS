// Kernel/SourceI/wm.c
#include "../Include/wm.h"

static int prev_left_button = 0;

// ============================================
// NỘI BỘ
// ============================================

static int point_in_process(process_t *p, int x, int y)
{
    if(x < p->x || x >= p->x + p->width)
        return 0;

    if(y < p->y || y >= p->y + p->height)
        return 0;

    return 1;
}

// ============================================
// API
// ============================================

int wm_bring_to_front(int id)
{
    process_t *p = process_get(id);
    int old_layer;
    int i;

    if(!p)
        return -1;

    if(p->layer == LAYER_NORMAL_MIN)
        return p->layer;

    old_layer = p->layer;

    // Cửa sổ vừa mở (chưa từng có layer hợp lệ) coi như đang đứng
    // "xa vô cực" -> mọi cửa sổ thường khác đều bị đẩy lùi +1 để
    // nhường chỗ layer 1 cho nó.
    if(old_layer < LAYER_NORMAL_MIN || old_layer > LAYER_NORMAL_MAX)
        old_layer = LAYER_DESKTOP;

    for(i = 0; i < PROCESS_MAX; i++)
    {
        process_t *q = process_get(i);

        if(!q || q == p)
            continue;

        if(q->layer < LAYER_NORMAL_MIN || q->layer > LAYER_NORMAL_MAX)
            continue;

        if(q->layer < old_layer)
            q->layer++;
    }

    p->layer = LAYER_NORMAL_MIN;

    return p->layer;
}

void wm_release_layer(int id)
{
    process_t *p = process_get(id);
    int old_layer;
    int i;

    if(!p)
        return;

    old_layer = p->layer;

    if(old_layer < LAYER_NORMAL_MIN || old_layer > LAYER_NORMAL_MAX)
        return;

    for(i = 0; i < PROCESS_MAX; i++)
    {
        process_t *q = process_get(i);

        if(!q || q == p)
            continue;

        if(q->layer < LAYER_NORMAL_MIN || q->layer > LAYER_NORMAL_MAX)
            continue;

        if(q->layer > old_layer)
            q->layer--;
    }

    p->layer = LAYER_DESKTOP;
}

void wm_handle_mouse(int mx, int my, int left_button)
{
    int clicked = (left_button && !prev_left_button);
    process_t *best = 0;
    int best_layer = LAYER_DESKTOP + 1;
    int i;

    prev_left_button = left_button;

    if(!clicked)
        return;

    for(i = 0; i < PROCESS_MAX; i++)
    {
        process_t *p = process_get(i);

        if(!p)
            continue;

        if(p->layer < LAYER_NORMAL_MIN || p->layer > LAYER_NORMAL_MAX)
            continue;

        if(!point_in_process(p, mx, my))
            continue;

        if(p->layer < best_layer)
        {
            best_layer = p->layer;
            best       = p;
        }
    }

    for(i = 0; i < PROCESS_MAX; i++)
    {
        process_t *p = process_get(i);

        if(!p)
            continue;

        p->focused = (p == best);
    }

    if(best)
        wm_bring_to_front(best->id);
}

void wm_composite(void (*desktop_draw)(void), void (*mouse_draw)(void))
{
    process_t *stack[PROCESS_MAX];
    int n = 0;
    int i;

    if(desktop_draw)
        desktop_draw();

    for(i = 0; i < PROCESS_MAX; i++)
    {
        process_t *p = process_get(i);

        if(!p)
            continue;

        if(p->layer < LAYER_NORMAL_MIN || p->layer > LAYER_NORMAL_MAX)
            continue;

        if(!p->visible)
            continue;

        stack[n++] = p;
    }

    // Insertion sort tăng dần theo layer (n <= PROCESS_MAX, nhỏ nên
    // không cần thuật toán phức tạp hơn).
    for(i = 1; i < n; i++)
    {
        process_t *key = stack[i];
        int j = i - 1;

        while(j >= 0 && stack[j]->layer > key->layer)
        {
            stack[j + 1] = stack[j];
            j--;
        }

        stack[j + 1] = key;
    }

    // Vẽ NGƯỢC từ layer lớn (xa) -> layer nhỏ (gần), để cửa sổ layer 1
    // luôn được vẽ SAU CÙNG trong nhóm cửa sổ, tức là nằm trên tất cả
    // các cửa sổ thường khác.
    for(i = n - 1; i >= 0; i--)
    {
        if(stack[i]->draw)
            stack[i]->draw(stack[i]);
    }

    if(mouse_draw)
        mouse_draw();
}
