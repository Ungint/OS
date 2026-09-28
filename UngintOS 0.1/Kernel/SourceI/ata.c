// Kernel/Source/ata.c
#include "../Include/ata.h"
#include "../Include/stdint.h"
#include "../Include/vga.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_BSY  0x80

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void ata_io_delay(void) {
    for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_CTRL);
}

// 🔥 SỬA: Thêm timeout để tránh treo cứng kernel nếu ổ đĩa không phản hồi
// (ví dụ: chạy fat32_init() với ổ Slave nhưng QEMU chỉ gắn 1 ổ đĩa)
#define ATA_TIMEOUT 100000

static uint8_t ata_wait_not_busy(void) {
    uint8_t status;
    uint32_t timeout = ATA_TIMEOUT;
    do {
        status = inb(ATA_PRIMARY_IO + 7);
    } while ((status & ATA_SR_BSY) && --timeout);
    return status;
}

static int ata_wait_drq(void) {
    uint8_t status = ata_wait_not_busy();
    uint32_t timeout = ATA_TIMEOUT;
    while (!(status & ATA_SR_DRQ) && timeout) {
        if (status & ATA_SR_ERR) return -1;
        status = inb(ATA_PRIMARY_IO + 7);
        timeout--;
    }
    if (!timeout) return -1; // 🔥 SỬA: hết thời gian chờ -> lỗi thay vì treo
    return 0;
}

// ============================================
// 🔥 SỬA: Hàm đọc sector có thêm tham số `drive`
// ============================================
int ata_read_sector(uint8_t drive, uint32_t lba, uint8_t *buffer) {
    if (ata_wait_not_busy() & ATA_SR_BSY) return -1; // 🔥 SỬA: bắt lỗi timeout sớm

    // drive: 0x80 = Master, 0x81 = Slave
    uint8_t drive_select = 0xE0 | ((drive & 0x01) << 4); // 0xE0 hoặc 0xF0
    outb(ATA_PRIMARY_IO + 6, drive_select | ((lba >> 24) & 0x0F));
    ata_io_delay();

    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));

    outb(ATA_PRIMARY_IO + 7, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    uint16_t *buf16 = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = inw(ATA_PRIMARY_IO);
    }

    return 0;
}

// ============================================
// 🔥 SỬA: Hàm đọc nhiều sector có thêm tham số `drive`
// ============================================
int ata_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, uint8_t *buffer) {
    for (uint32_t i = 0; i < count; i++) {
        if (ata_read_sector(drive, lba + i, buffer + i * 512) != 0) {
            return -1;
        }
    }
    return 0;
}

// ============================================
// 🔥 SỬA: Hàm ghi giờ nhận tham số `drive`, đồng bộ với các hàm đọc
// (trước đây luôn ghi cứng vào Master 0x80, trong khi FAT32 lại đọc
//  từ Slave 0x81 -> nếu bật ghi file sẽ ghi nhầm ổ, có thể phá boot disk)
// ============================================
int ata_write_sector(uint8_t drive, uint32_t lba, const uint8_t *buffer) {
    ata_wait_not_busy();

    uint8_t drive_select = 0xE0 | ((drive & 0x01) << 4);
    outb(ATA_PRIMARY_IO + 6, drive_select | ((lba >> 24) & 0x0F));
    ata_io_delay();

    outb(ATA_PRIMARY_IO + 2, 1);
    outb(ATA_PRIMARY_IO + 3, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + 4, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + 5, (uint8_t)(lba >> 16));

    outb(ATA_PRIMARY_IO + 7, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) {
        return -1;
    }

    const uint16_t *buf16 = (const uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_IO, buf16[i]);
    }

    outb(ATA_PRIMARY_IO + 7, 0xE7);
    ata_wait_not_busy();

    return 0;
}