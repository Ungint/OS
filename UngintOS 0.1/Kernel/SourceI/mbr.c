// Kernel/Source/mbr.c
#include "../Include/mbr.h"
#include "../Include/ata.h"
#include "../Include/vga.h"
#include "../Include/string.h"

static mbr_t mbr;

// ============================================
// 🔥 SỬA: Hàm init nhận tham số drive
// ============================================
int mbr_init(uint8_t drive) {
    uint8_t buffer[512];

    print("  MBR: Reading MBR from drive 0x", VGA_COLOR_WHITE, VGA_BG_BLACK);
    print_hex8(drive);
    print(" LBA 0...\n", VGA_COLOR_WHITE, VGA_BG_BLACK);

    // 🔥 SỬA: Dùng drive được truyền vào
    if (ata_read_sector(drive, 0, buffer) != 0) {
        print("  MBR: Failed to read LBA 0!\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return -1;
    }

    mbr = *(mbr_t*)buffer;

    if (mbr.signature != MBR_SIGNATURE) {
        print("  MBR: Invalid MBR signature (0x", VGA_COLOR_RED, VGA_BG_BLACK);
        print_hex(mbr.signature);
        print(")!\n", VGA_COLOR_RED, VGA_BG_BLACK);
        return -1;
    }

    print("  MBR: Valid MBR signature (0xAA55)\n", VGA_COLOR_GREEN, VGA_BG_BLACK);
    return 0;
}

void mbr_print_partitions(void) {
    print("  MBR Partitions:\n", VGA_COLOR_CYAN, VGA_BG_BLACK);

    for (int i = 0; i < 4; i++) {
        mbr_partition_t *p = &mbr.partitions[i];

        if (p->type == 0) continue;

        print("    Partition ", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print_dec(i + 1);
        print(": type 0x", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print_hex(p->type);
        print(" (LBA ", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print_hex(p->lba_start);
        print(" -> ", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print_hex(p->lba_start + p->sector_count - 1);
        print(", ", VGA_COLOR_WHITE, VGA_BG_BLACK);
        print_dec((uint32_t)(p->sector_count * 512 / 1024 / 1024));
        print(" MB)\n", VGA_COLOR_WHITE, VGA_BG_BLACK);
    }
}

int mbr_find_partition(uint8_t type, uint32_t *lba_start, uint32_t *sector_count) {
    for (int i = 0; i < 4; i++) {
        mbr_partition_t *p = &mbr.partitions[i];
        if (p->type == type) {
            *lba_start = p->lba_start;
            *sector_count = p->sector_count;
            return 0;
        }
    }
    return -1;
}