// Kernel/Source/boot_data.c
#include "../Include/boot_data.h"

uint8_t  vbe_ok = 0;
uint32_t boot_fb = 0;
uint32_t boot_pitch = 0;
uint16_t boot_width = 0;
uint16_t boot_height = 0;
uint8_t  boot_bpp = 0;

// ============================================
// 🔥 SỬA: Trước đây các biến trên LUÔN = 0 vì "extern" không hề nối
// được với các biến CÙNG TÊN bên boot2.asm (boot2.asm build ra file
// .bin thô riêng biệt, không chia sẻ symbol với kernel ELF).
// Bây giờ đọc trực tiếp từ struct mà boot2.asm đã ghi vào địa chỉ vật
// lý cố định 0x6000 ngay trước khi nhảy vào kernel (xem boot2.asm,
// nhãn .continue_boot). Layout struct (14 byte):
//   offset 0  : uint32 boot_fb
//   offset 4  : uint32 boot_pitch
//   offset 8  : uint16 boot_width
//   offset 10 : uint16 boot_height
//   offset 12 : uint8  boot_bpp
//   offset 13 : uint8  vbe_ok
// ============================================
void boot_data_load(void) {
    volatile uint8_t *info = (volatile uint8_t *)0x6000;

    boot_fb     = *(volatile uint32_t *)(info + 0);
    boot_pitch  = *(volatile uint32_t *)(info + 4);
    boot_width  = *(volatile uint16_t *)(info + 8);
    boot_height = *(volatile uint16_t *)(info + 10);
    boot_bpp    = *(volatile uint8_t  *)(info + 12);
    vbe_ok      = *(volatile uint8_t  *)(info + 13);
}
