// Kernel/Include/boot_data.h
#ifndef BOOT_DATA_H
#define BOOT_DATA_H

#include "stdint.h"

extern uint8_t  vbe_ok;
extern uint32_t boot_fb;
extern uint32_t boot_pitch;
extern uint16_t boot_width;
extern uint16_t boot_height;
extern uint8_t  boot_bpp;

// 🔥 THÊM: đọc thông tin VBE thật từ struct mà boot2.asm đã ghi vào
// địa chỉ vật lý cố định 0x6000 trước khi nhảy vào kernel. PHẢI gọi
// hàm này đầu tiên trong _start(), trước khi dùng vbe_ok/boot_fb/...
void boot_data_load(void);

#endif
