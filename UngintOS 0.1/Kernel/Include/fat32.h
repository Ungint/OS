// Kernel/Include/fat32.h
#ifndef FAT32_H
#define FAT32_H

#include "stdint.h"

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

#define MAX_PATH 128
#define FAT32_EOC_MIN 0x0FFFFFF8

typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t ctime_ms;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t first_cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

int fat32_init(uint8_t drive);
int fat32_ls(void);
int fat32_open(const char *filename);
int fat32_read(void *buffer, uint32_t bytes);
void fat32_close(void);

// 🔥 THÊM: tạo/ghi đè file trong thư mục hiện tại rồi mở để ghi
int fat32_create(const char *filename);
// 🔥 THÊM: ghi dữ liệu vào file đang mở bằng fat32_create()
int fat32_write(const void *buffer, uint32_t bytes);
// 🔥 THÊM: đổi thư mục hiện tại (dùng cho lệnh "tp" trong shell)
int fat32_chdir(const char *path);

// 🔥 HÀM MỚI: Kiểm tra thư mục có tồn tại không
int fat32_dir_exists(const char *path);

// 🔥 THÊM: liệt kê tên file (không đệ quy) trong 1 thư mục ra mảng, phục vụ
// cho việc quét thư mục Font/ tìm file *.f, hoặc các mục đích khác cần
// duyệt thư mục bằng code thay vì chỉ in ra màn hình (như fat32_ls()).
// - path: đường dẫn thư mục cần liệt kê (vd "/Font")
// - names_out: mảng buffer chứa tên file/thư mục (mỗi tên tối đa name_cap byte)
// - name_cap: kích thước mỗi buffer tên (vd 64)
// - is_dir_out: mảng int, 1 nếu entry là thư mục, 0 nếu là file (có thể NULL)
// - sizes_out: mảng uint32_t, kích thước file (có thể NULL)
// - max_entries: số lượng entry tối đa mà các mảng trên chứa được
// Trả về: số entry tìm thấy (>=0), hoặc -1 nếu lỗi.
int fat32_list_dir(const char *path, char *names_out, int name_cap,
                    int *is_dir_out, uint32_t *sizes_out, int max_entries);
uint32_t fat32_file_size(void);
#endif