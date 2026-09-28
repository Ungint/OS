#include "../Include/memory.h"

#define HEAP_START 0x300000
#define HEAP_END   0x10000000

static uint32_t heap_pos=HEAP_START;

void memory_init(void) {
    heap_pos=HEAP_START;
}

void *malloc(uint32_t size) {
    if(size==0)return 0;

    size=(size+7)&~7;

    if(heap_pos+size>HEAP_END)
        return 0;

    void *ptr=(void*)(uintptr_t)heap_pos;

    heap_pos+=size;

    return ptr;
}

void free(void *ptr) {
    (void)ptr;
}