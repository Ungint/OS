// Kernel/Include/mbr.h
#ifndef MBR_H
#define MBR_H

#include "stdint.h"

#define MBR_SIGNATURE 0xAA55

typedef struct {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) mbr_partition_t;

typedef struct {
    uint8_t bootstrap_code[446];
    mbr_partition_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

// 🔥 SỬA: Thêm tham số drive
int mbr_init(uint8_t drive);
void mbr_print_partitions(void);
int mbr_find_partition(uint8_t type, uint32_t *lba_start, uint32_t *sector_count);

#endif