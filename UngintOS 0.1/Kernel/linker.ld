ENTRY(_start)

SECTIONS
{
    . = 0x20000;

    .text :
    {
        KEEP(*(.text._start))
        *(.text*)
    }

    .rodata :
    {
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
    }

    /* 🔥 THÊM (NOLOAD): image.c/video.c/font.c dùng các buffer static khá
       lớn (vài trăm KB - vài MB) để nạp ảnh/video/font từ FAT32. Nếu để
       .bss là section thường, "objcopy -O binary" sẽ ghi ĐẦY zero-byte
       cho toàn bộ vùng .bss vào thẳng kernel.bin (vì nó lấp đầy khoảng
       trống liên tục giữa các section theo địa chỉ ảo) -> kernel.bin sẽ
       phình to hơn rất nhiều so với 96 sector (49152 byte) mà boot2.asm
       đọc từ đĩa lúc boot (xem `dap` trong Boot/boot2.asm) -> kernel bị
       cắt cụt / boot lỗi. Đánh dấu (NOLOAD) để .bss KHÔNG được ghi vào
       file nhị phân; vùng nhớ đó sẽ chỉ tồn tại trong RAM lúc chạy
       (không cần khởi tạo sẵn nội dung vì mọi module đều tự ghi dữ liệu
       thật vào buffer trước khi dùng, không phụ thuộc buffer = 0 sẵn).
    */
    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    }
}