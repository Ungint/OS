// Kernel/Include/video.h
// ============================================
// NẠP & PHÁT VIDEO RAW TỪ FAT32 (loadvideo / usevideo)
// Mỗi khung hình có cùng định dạng pixel với image.h (RGB/RGBA thô),
// được đọc và vẽ TỪNG KHUNG MỘT trực tiếp từ đĩa (không nạp cả video
// vào RAM cùng lúc) để hỗ trợ video dài mà không cần buffer khổng lồ.
// ============================================
#ifndef VIDEO_H
#define VIDEO_H

#include "stdint.h"

// Header file video ".vid" (14 byte, packed):
//   offset 0  : char magic[4]    = "MVID"
//   offset 4  : uint16 width
//   offset 6  : uint16 height
//   offset 8  : uint8  bpp        (24 hoặc 32, giống image.h)
//   offset 9  : uint8  reserved   (= 0)
//   offset 10 : uint16 frame_count
//   offset 12 : uint16 delay_ms   (thời gian giữa 2 khung hình, mili-giây)
// Theo sau header là frame_count khung hình, mỗi khung width*height*(bpp/8)
// byte pixel liên tiếp nhau (không có gì xen giữa các khung).
typedef struct {
    uint8_t magic[4];
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint8_t reserved;
    uint16_t frame_count;
    uint16_t delay_ms;
} __attribute__((packed)) video_header_t;

// Nạp (thẩm định) 1 file video: mở file, đọc & kiểm tra header, rồi đóng
// file lại ngay (chưa đọc khung hình nào). Trả về 0 nếu hợp lệ, -1 nếu lỗi.
int loadvideo(const char *path);

// Phát video ĐÃ NẠP (bằng loadvideo) tại toạ độ (x, y): mở lại file, đọc
// và vẽ tuần tự từng khung hình, chờ delay_ms giữa mỗi khung, rồi đóng
// file. Hàm này BLOCKING cho tới khi phát xong toàn bộ video.
// Trả về 0 nếu OK, -1 nếu chưa loadvideo() hoặc VBE chưa sẵn sàng.
int usevideo(uint32_t x, uint32_t y);

uint16_t video_width(void);
uint16_t video_height(void);
uint16_t video_frame_count(void);

#endif
