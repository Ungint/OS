#ifndef IMAGE_H
#define IMAGE_H

#include "stdint.h"

typedef struct
{
    uint32_t *pixels;
    uint16_t width;
    uint16_t height;
    uint32_t size;
    int loaded;
    int has_alpha;
} IMAGE;

typedef enum
{
    IMG_OK = 0,
    IMG_ERR_OPEN,
    IMG_ERR_READ,
    IMG_ERR_INVALID,
    IMG_ERR_UNSUPPORTED,
    IMG_ERR_MEMORY,
    IMG_ERR_UNKNOWN_FORMAT
} image_error_t;

IMAGE loadimage(const char *path);

int useimage(
    const IMAGE *image,
    uint32_t x,
    uint32_t y
);

void freeimage(
    IMAGE *image
);

uint16_t image_width(
    const IMAGE *image
);

uint16_t image_height(
    const IMAGE *image
);

const uint32_t *image_pixels_ptr(
    const IMAGE *image
);

image_error_t loadimage_last_error(void);

#endif