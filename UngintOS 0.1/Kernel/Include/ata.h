// Kernel/Include/ata.h
#ifndef ATA_H
#define ATA_H

#include "stdint.h"

// 🔥 SỬA: Thêm tham số `drive` vào tất cả hàm đọc
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t *buffer);
int ata_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, uint8_t *buffer);

// 🔥 SỬA: Hàm ghi giờ cũng nhận `drive`, đồng bộ với hàm đọc
int ata_write_sector(uint8_t drive, uint32_t lba, const uint8_t *buffer);

#endif