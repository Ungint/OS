// Kernel/SourceI/video.c
#include "../Include/video.h"
#include "../Include/gfx.h"
#include "../Include/fat32.h"
#include "../Include/string.h"

#define VIDEO_MAX_FRAME_BYTES (1024u * 1024u)
#define VIDEO_MAX_PATH 128

static uint8_t frame_buf[VIDEO_MAX_FRAME_BYTES];
static char video_path[VIDEO_MAX_PATH];
static video_header_t video_hdr;
static int video_loaded = 0;

uint16_t video_width(void)       { return video_loaded ? video_hdr.width : 0; }
uint16_t video_height(void)      { return video_loaded ? video_hdr.height : 0; }
uint16_t video_frame_count(void) { return video_loaded ? video_hdr.frame_count : 0; }

int loadvideo(const char *path) {
    video_loaded = 0;

    if (fat32_open(path) != 0) {
        return -1;
    }

    video_header_t hdr;
    int n = fat32_read(&hdr, sizeof(hdr));
    fat32_close();

    if (n != (int)sizeof(hdr)) return -1;

    if (hdr.magic[0] != 'M' || hdr.magic[1] != 'V' ||
        hdr.magic[2] != 'I' || hdr.magic[3] != 'D') {
        return -1;
    }
    if (hdr.bpp != 24 && hdr.bpp != 32) return -1;
    if (hdr.frame_count == 0) return -1;

    uint32_t bytes_per_pixel = hdr.bpp / 8;
    uint32_t frame_size = (uint32_t)hdr.width * (uint32_t)hdr.height * bytes_per_pixel;
    if (frame_size == 0 || frame_size > VIDEO_MAX_FRAME_BYTES) return -1;

    int len = strlen(path);
    if (len >= VIDEO_MAX_PATH) return -1;

    strcpy(video_path, path);
    video_hdr = hdr;
    video_loaded = 1;

    return 0;
}

int usevideo(uint32_t x, uint32_t y) {
    if (!video_loaded) return -1;
    if (!gfx_ready()) return -1;

    if (fat32_open(video_path) != 0) return -1;

    // Bỏ qua header, quay lại đúng vị trí bắt đầu dữ liệu khung hình.
    video_header_t skip;
    if (fat32_read(&skip, sizeof(skip)) != (int)sizeof(skip)) {
        fat32_close();
        return -1;
    }

    uint32_t bytes_per_pixel = video_hdr.bpp / 8;
    uint32_t frame_size = (uint32_t)video_hdr.width * (uint32_t)video_hdr.height * bytes_per_pixel;

    for (uint16_t f = 0; f < video_hdr.frame_count; f++) {
        int n = fat32_read(frame_buf, frame_size);
        if (n != (int)frame_size) break; // hết file sớm hơn dự kiến

        for (uint32_t row = 0; row < video_hdr.height; row++) {
            const uint8_t *row_ptr = frame_buf + (uint32_t)row * video_hdr.width * bytes_per_pixel;
            for (uint32_t col = 0; col < video_hdr.width; col++) {
                const uint8_t *px = row_ptr + (uint32_t)col * bytes_per_pixel;
                uint32_t color = ((uint32_t)px[0] << 16) | ((uint32_t)px[1] << 8) | (uint32_t)px[2];
                gfx_putpixel(x + col, y + row, color);
            }
        }

        gfx_delay_ms(video_hdr.delay_ms ? video_hdr.delay_ms : 100);
    }

    fat32_close();
    return 0;
}
