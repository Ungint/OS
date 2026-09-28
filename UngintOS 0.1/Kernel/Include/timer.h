#ifndef TIMER_H
#define TIMER_H

#include "../Include/stdint.h"

// Khởi tạo timer — calibrate TSC bằng RTC
// Gọi 1 lần lúc boot, mất ~2 giây
void timer_init(void);

// Trả về số tick (ms) từ lúc boot
// Dùng để đo thời gian, FPS, delay, v.v.
uint64_t timer_ms(void);

// Trả về CPU frequency (Hz) sau khi calibrate
uint64_t timer_cpu_hz(void);

#endif