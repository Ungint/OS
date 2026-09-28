org 0x8000
[BITS 16]

; ================================================
; ENTRY POINT
; ================================================
main:
    call clear
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; ---- Try VBE ----
    call vbe_init
    
    cmp byte [vbe_ok], 1
    jne .vbe_failed

    ; ---- VBE OK ----
    call clear
    mov si, msg_vbe_ok
    call print

    mov si, msg_fb
    call print
    mov eax, [boot_fb]
    call print_hex32
    mov si, msg_nl
    call print

    mov si, msg_pitch
    call print
    mov eax, [boot_pitch]
    call print_hex32
    mov si, msg_nl
    call print

    mov si, msg_res
    call print
    movzx eax, word [boot_width]
    call print_dec32
    mov si, msg_x
    call print
    movzx eax, word [boot_height]
    call print_dec32
    mov si, msg_nl
    call print

    mov si, msg_bpp
    call print
    movzx eax, byte [boot_bpp]
    call print_dec32
    mov si, msg_nl
    call print

    jmp .continue_boot

.vbe_failed:
    mov si, msg_vbe_failed
    call print

.continue_boot:
    ; ============================================
    ; 🔥 SỬA: Ghi thông tin VBE THẬT SỰ vào địa chỉ vật lý cố định 0x6000.
    ; Trước đây code chỉ copy 1 buffer rỗng (vbe_info, không bao giờ được
    ; ghi dữ liệu) sang 0x6000, còn kernel lại dùng "extern" trỏ tới biến
    ; boot_fb/vbe_ok riêng của NÓ (luôn = 0, vì 2 file build tách biệt,
    ; không hề chia sẻ symbol với boot2.asm) -> kernel không bao giờ biết
    ; VBE đã bật hay chưa, mọi hàm vẽ vì vậy return ngay không vẽ gì.
    ; Bây giờ kernel (boot_data.c) sẽ ĐỌC đúng struct này tại 0x6000.
    ; ============================================
    mov eax, [boot_fb]
    mov [0x6000], eax
    mov eax, [boot_pitch]
    mov [0x6004], eax
    mov ax, [boot_width]
    mov [0x6008], ax
    mov ax, [boot_height]
    mov [0x600A], ax
    mov al, [boot_bpp]
    mov [0x600C], al
    mov al, [vbe_ok]
    mov [0x600D], al


    ; ---- Boot drive ----
    mov si, msg1
    call print

    cmp dl, 0
    jne .drive_ok
    mov dl, 0x80

.drive_ok:
    mov [boot_drive], dl

    ; ---- Load kernel (120 sectors) ----
    ; 🔥 SỬA: tăng từ 96 -> 120 sector (49152 -> 61440 byte) để có thêm chỗ
    ; cho code mới (gfx.c/font.c/image.c/video.c). VẪN phải <= 128 sector
    ; (65536 byte = giới hạn 1 segment real-mode 0x2000:xxxx dùng bởi DAP
    ; bên dưới) - KHÔNG được vượt quá nếu không buffer sẽ bị tràn segment.
    ; Lưu ý: các buffer lớn (ảnh/video/font nạp từ FAT32) nằm trong .bss
    ; (NOLOAD, xem Kernel/linker.ld) nên KHÔNG tính vào kích thước file
    ; kernel.bin - chỉ code thật (.text/.rodata/.data) mới cần vừa 120 sector.
    mov di, 5
.retry_read:
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc .read_success

    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    dec di
    jnz .retry_read
    jmp .disk_error

.read_success:
    mov si, msg_kernel_loaded
    call print

    ; ---- Enable A20 ----
    cli
    in al, 0x92
    or al, 2
    out 0x92, al

    ; ---- Load GDT ----
    lgdt [gdt_desc]

    ; ---- Enter Protected Mode ----
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_start

.disk_error:
    mov si, msg_err
    call print
.hang:
    hlt
    jmp .hang

; ================================================
; VBE INIT (FIXED)
; ================================================
vbe_init:
    pusha
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov byte [vbe_ok], 0

    mov si, msg_vbe_check
    call print

    ; ---- Get VBE Info ----
    mov ax, 0x4F00
    mov di, 0x5000
    int 0x10
    
    cmp ax, 0x004F
    jne .failed
    
    cmp dword [0x5000], 'VESA'
    jne .failed

    mov si, msg_vbe_found
    call print

    ; ---- Get mode list ----
    mov bx, [0x5000 + 0x0E]
    mov dx, [0x5000 + 0x10]
    mov es, dx
    mov si, bx

.next_mode:
    mov cx, [es:si]
    cmp cx, 0xFFFF
    je .failed

    push es
    push si
    push cx

    xor ax, ax
    mov es, ax
    mov di, 0x5200
    mov ax, 0x4F01
    int 0x10

    pop cx
    pop si
    pop es

    cmp ax, 0x004F
    jne .next_mode_advance

    ; ---- Check attributes ----
    mov ax, [0x5200]
    test ax, 1
    jz .next_mode_advance
    test ax, 16
    jz .next_mode_advance

    ; ---- Try 1280x720 first ----
    cmp word [0x5200 + 0x12], 1280
    jne .try_1024
    cmp word [0x5200 + 0x14], 720
    jne .try_1024
    cmp byte [0x5200 + 0x19], 32
    je .found_mode

.try_1024:
    cmp word [0x5200 + 0x12], 1024
    jne .try_800
    cmp word [0x5200 + 0x14], 768
    jne .try_800
    cmp byte [0x5200 + 0x19], 32
    je .found_mode

.try_800:
    cmp word [0x5200 + 0x12], 800
    jne .try_640
    cmp word [0x5200 + 0x14], 600
    jne .try_640
    cmp byte [0x5200 + 0x19], 32
    je .found_mode

.try_640:
    cmp word [0x5200 + 0x12], 640
    jne .next_mode_advance
    cmp word [0x5200 + 0x14], 480
    jne .next_mode_advance
    cmp byte [0x5200 + 0x19], 32
    jne .next_mode_advance

.found_mode:
    mov [vbe_mode], cx
    
    mov eax, [0x5200 + 0x28]
    mov [boot_fb], eax
    
    xor eax, eax
    mov ax, [0x5200 + 0x10]
    mov [boot_pitch], eax
    
    mov ax, [0x5200 + 0x12]
    mov [boot_width], ax
    
    mov ax, [0x5200 + 0x14]
    mov [boot_height], ax
    
    mov al, [0x5200 + 0x19]
    mov [boot_bpp], al

    ; ---- Set VBE mode (mode chuẩn VESA lấy được từ mode list) ----
    mov ax, 0x4F02
    mov bx, [vbe_mode]
    or bx, 0x4000
    int 0x10

    cmp ax, 0x004F
    jne .failed

    ; ============================================
    ; 🔥 THÊM: Ép độ phân giải thật lên 1280x720x32 bằng Bochs Dispi
    ; Interface (BGA - phần cứng ảo QEMU/Bochs dùng cho VBE, khác với
    ; danh sách mode cố định trong VESA BIOS ROM). Mode list của BIOS
    ; chỉ có các mode chuẩn 4:3 (640x480, 800x600, 1024x768...), KHÔNG
    ; có 1280x720 - đó là lý do trước đây luôn rơi xuống 640x480.
    ; BGA cho set X/Y TÙY Ý (X chia hết cho 8) qua 2 port I/O riêng,
    ; không phụ thuộc mode list. PhysBasePtr (boot_fb) là địa chỉ PCI
    ; BAR, KHÔNG đổi theo độ phân giải nên vẫn dùng lại giá trị đã lấy
    ; được ở trên từ mode VESA chuẩn.
    ; ============================================
    call bga_force_1280x720x32

    mov byte [vbe_ok], 1
    mov si, msg_vbe_ok
    call print
    popa
    ret

.next_mode_advance:
    add si, 2
    jmp .next_mode

.failed:
    mov byte [vbe_ok], 0
    mov si, msg_vbe_failed
    call print
    popa
    ret

; ================================================
; 🔥 THÊM: Ép độ phân giải BGA lên 1280x720x32 qua I/O port 0x1CE/0x1CF,
; bỏ qua giới hạn của danh sách mode VESA BIOS.
; ================================================
bga_force_1280x720x32:
    pusha

    ; Tắt trước khi đổi mode (khuyến cáo theo spec BGA)
    mov dx, 0x01CE
    mov ax, 4               ; VBE_DISPI_INDEX_ENABLE
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0
    out dx, ax

    ; XRES = 1280
    mov dx, 0x01CE
    mov ax, 1               ; VBE_DISPI_INDEX_XRES
    out dx, ax
    mov dx, 0x01CF
    mov ax, 1280
    out dx, ax

    ; YRES = 720
    mov dx, 0x01CE
    mov ax, 2               ; VBE_DISPI_INDEX_YRES
    out dx, ax
    mov dx, 0x01CF
    mov ax, 720
    out dx, ax

    ; BPP = 32
    mov dx, 0x01CE
    mov ax, 3               ; VBE_DISPI_INDEX_BPP
    out dx, ax
    mov dx, 0x01CF
    mov ax, 32
    out dx, ax

    ; Bật lại + bật Linear Framebuffer (ENABLE=1 | LFB_ENABLED=0x40)
    mov dx, 0x01CE
    mov ax, 4               ; VBE_DISPI_INDEX_ENABLE
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0x41
    out dx, ax

    ; Cập nhật lại thông tin THẬT cho kernel: pitch của BGA luôn đúng
    ; bằng XRES * (BPP/8), không có padding thêm.
    mov dword [boot_pitch], 1280*4
    mov word [boot_width], 1280
    mov word [boot_height], 720
    mov byte [boot_bpp], 32

    popa
    ret

; ================================================
; PRINT FUNCTIONS
; ================================================
print_hex16:
    push ax
    push cx
    push dx
    mov cx, 4
.loop:
    rol bx, 4
    mov al, bl
    and al, 0x0F
    cmp al, 10
    jb .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov ah, 0x0E
    int 0x10
    loop .loop
    pop dx
    pop cx
    pop ax
    ret

print_hex32:
    pusha
    mov bx, ax
    shr eax, 16
    mov bx, ax
    call print_hex16
    popa
    pusha
    mov bx, ax
    call print_hex16
    popa
    ret

print_dec32:
    pusha
    mov ecx, 10
    xor ebx, ebx
    mov edi, 10
    mov esi, 0
.loop:
    xor edx, edx
    div ecx
    push dx
    inc esi
    test eax, eax
    jnz .loop
.print:
    pop dx
    add dl, '0'
    mov ah, 0x0E
    int 0x10
    dec esi
    jnz .print
    popa
    ret

clear:
    pusha
    mov ah, 0x06
    mov al, 0x00
    mov bh, 0x07
    mov cx, 0x0000
    mov dx, 0x184F
    int 0x10
    mov ah, 0x02
    mov bh, 0x00
    mov dh, 0x00
    mov dl, 0x00
    int 0x10
    popa
    ret

print:
    push ax
    push si
.loop:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    pop si
    pop ax
    ret

; ================================================
; DATA
; ================================================
align 4
dap:
    db 0x10
    db 0
    dw 120
    dw 0x0000
    dw 0x2000
    dq 17

boot_drive  db 0
vbe_ok      db 0
vbe_mode    dw 0

boot_fb     dd 0
boot_pitch  dd 0
boot_width  dw 0
boot_height dw 0
boot_bpp    db 0

align 4
vbe_info:
    times 512 db 0

align 4
vbe_mode_info:
    times 256 db 0

; ================================================
; MESSAGES
; ================================================
msg1                db "Stage2 Loaded!", 0x0D, 0x0A, 0
msg_kernel_loaded   db "Kernel Loaded to 0x20000!", 0x0D, 0x0A, 0
msg_err             db "Disk Read Error!", 0x0D, 0x0A, 0

msg_vbe_check       db "VBE: checking...", 0x0D, 0x0A, 0
msg_vbe_found       db "VBE: found VESA", 0x0D, 0x0A, 0
msg_vbe_ok          db "VBE: OK", 0x0D, 0x0A, 0
msg_vbe_failed      db "VBE: FAILED", 0x0D, 0x0A, 0

msg_fb              db "Framebuffer: 0x", 0
msg_pitch           db "Pitch: 0x", 0
msg_res             db "Resolution: ", 0
msg_bpp             db "BPP: ", 0
msg_x               db " x ", 0
msg_nl              db 0x0D, 0x0A, 0

; ================================================
; GDT
; ================================================
gdt_start:
    dd 0x00000000, 0x00000000
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
    dw 0x0000, 0x0000
    db 0x00, 0x9A, 0xAF, 0x00
    dw 0x0000, 0x0000
    db 0x00, 0x92, 0x00, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ================================================
; 32-BIT PROTECTED MODE
; ================================================
[BITS 32]
protected_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; 🔥 SỬA: chỉ ghi text-mode debug (0xB8000) khi VBE THẤT BẠI - lúc đó
    ; màn hình vẫn ở text mode thật nên còn nhìn thấy được. Nếu VBE đã
    ; bật thành công thì 0xB8000 không còn là bộ nhớ hiển thị nữa, ghi
    ; vào đó vô nghĩa -> bỏ qua hoàn toàn (không dùng text mode nữa).
    cmp byte [vbe_ok], 1
    je .skip_msg2
    mov esi, msg2
    mov edi, 0xB8000 + 80*2*2
    call print32
.skip_msg2:

    call enable_long_mode
    jmp 0x18:long_mode_start

enable_long_mode:
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    call setup_paging

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

setup_paging:
    pushad

    ; ============================================
    ; 🔥 THÊM: Lập trình lại PAT (Page Attribute Table, MSR 0x277) để có
    ; 1 slot Write-Combining (WC) - đây là kiểu cache mà driver GPU thật
    ; (Linux/Windows) luôn dùng cho framebuffer, KHÔNG dùng UC thuần.
    ;
    ; Lý do: UC thuần (Uncacheable) bắt MỌI write phải hoàn thành tuần
    ; tự từng cái một, cực chậm khi copy nguyên màn hình (hàng triệu
    ; write nhỏ). WC thì gộp nhiều write liên tiếp thành 1 burst trước
    ; khi đẩy ra thật - vừa nhanh gần bằng cache thường, vừa vẫn đảm
    ; bảo dữ liệu được đẩy ra VRAM thật đều đặn (không bị
    ; "kẹt" trong cache CPU như kiểu Write-Back mặc định ban đầu
    ; từng gây ra bug "màn hình gần như đứng hình" trước đó).
    ;
    ; PAT có 8 slot (PA0..PA7), chọn bằng 3 bit cờ trang: PAT(bit12 với
    ; trang 2MB) | PCD(bit4) | PWT(bit3). Giá trị mặc định lúc reset CPU:
    ;   PA0=WB(06) PA1=WT(04) PA2=UC-(07) PA3=UC(00)
    ;   PA4=WB(06) PA5=WT(04) PA6=UC-(07) PA7=UC(00)
    ; -> Không có slot nào là WC(01) cả nên phải ghi đè PA1 thành WC.
    ; Việc này không ảnh hưởng gì đến trang thường (PAT=0,PCD=0,
    ; PWT=0 -> slot PA0=WB, vẫn y như cũ).
    ; ============================================
    mov ecx, 0x277
    mov eax, 0x00070106
    mov edx, 0x00070406
    wrmsr

    ; ============================================
    ; 🔥 SỬA: Trước đây chỉ identity-map 1GB đầu (0x00000000-0x3FFFFFFF).
    ; Framebuffer VBE (boot_fb) thường nằm ở địa chỉ PCI BAR RẤT CAO
    ; (thường > 1GB trên QEMU) -> ghi pixel vào đó trước đây sẽ gây
    ; page fault / triple fault ngay lập tức (đây là nguyên nhân gây
    ; "lỗi tùm lum"/crash khi bật VBE). Giờ map đủ 4GB (4 Page Directory,
    ; mỗi cái phủ 1GB bằng trang 2MB) để chắc chắn phủ được boot_fb.
    ; ============================================

    ; ---- Xóa PML4 (0x1000) và PDPT (0x2000) ----
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0x2000 / 4
    rep stosd

    ; ---- Xóa 4 Page Directory: 0x4000, 0x11000, 0x12000, 0x13000 ----
    mov edi, 0x4000
    xor eax, eax
    mov ecx, 0x1000 / 4
    rep stosd
    mov edi, 0x11000
    xor eax, eax
    mov ecx, 0x1000 / 4
    rep stosd
    mov edi, 0x12000
    xor eax, eax
    mov ecx, 0x1000 / 4
    rep stosd
    mov edi, 0x13000
    xor eax, eax
    mov ecx, 0x1000 / 4
    rep stosd

    ; ---- PML4[0] -> PDPT ----
    mov dword [0x1000], 0x2000 | 0x03

    ; ---- PDPT[0..3] -> 4 Page Directory (mỗi cái phủ 1GB) ----
    mov dword [0x2000 + 0*8], 0x4000  | 0x03   ; GB0: 0x00000000-0x3FFFFFFF
    mov dword [0x2000 + 1*8], 0x11000 | 0x03   ; GB1: 0x40000000-0x7FFFFFFF
    mov dword [0x2000 + 2*8], 0x12000 | 0x03   ; GB2: 0x80000000-0xBFFFFFFF
    mov dword [0x2000 + 3*8], 0x13000 | 0x03   ; GB3: 0xC0000000-0xFFFFFFFF

    ; ---- Đổ 512 entry trang 2MB cho từng Page Directory ----
    mov edi, 0x4000
    mov eax, 0x00000083
    mov ecx, 512
.loop_gb0:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .loop_gb0

    mov edi, 0x11000
    mov eax, 0x40000083
    mov ecx, 512
.loop_gb1:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .loop_gb1

    mov edi, 0x12000
    mov eax, 0x80000083
    mov ecx, 512
.loop_gb2:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .loop_gb2

    mov edi, 0x13000
    mov eax, 0xC0000083
    mov ecx, 512
.loop_gb3:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
    loop .loop_gb3

    mov eax, 0x1000
    mov cr3, eax

    ; ============================================
    ; 🔥 THÊM: đánh dấu vùng FRAMEBUFFER VBE là UNCACHEABLE (PCD=1)
    ;
    ; NGUYÊN NHÂN GÂY "RENDER SIÊU CHẬM":
    ; Toàn bộ 4GB ở trên được map với cờ 0x83 (Present+Write+PS), tức là
    ; KHÔNG bật PCD -> CPU coi cả framebuffer là RAM thường, cacheable.
    ; Khi kernel ghi pixel (gfx_present/memcpy), dữ liệu chỉ nằm trong
    ; cache của CPU; CPU không có lý do gì để tự flush cache xuống vùng
    ; nhớ VRAM thật ngay lập tức - nó chỉ "trôi" xuống khi cache line đó
    ; bị đẩy ra (do thiếu chỗ cache ở nơi khác). Kết quả: hình ảnh hiển
    ; thị (QEMU đọc thẳng từ VRAM thật, không qua cache CPU) chỉ được
    ; cập nhật một cách RẤT RẢI RÁC -> chính là hiện tượng "render chậm
    ; như rùa" / vài khung hình mỗi... phút mà bro gặp phải.
    ;
    ; Sửa: quét PML4->PDPT->PD để tìm đúng (các) page 2MB chứa boot_fb,
    ; rồi đánh dấu (các) page đó là Write-Combining (WC, qua PAT ở trên)
    ; thay vì Write-Back mặc định. Phần còn lại của bộ nhớ (code/data/
    ; stack) vẫn cacheable Write-Back như cũ -> vẫn nhanh.
    ; ============================================
    mov eax, [boot_fb]
    and eax, 0xFFE00000         ; làm tròn xuống bội số 2MB
    mov ebx, eax                ; ebx = địa chỉ base 2MB-aligned của framebuffer

    mov edx, eax
    shr edx, 30
    and edx, 3                  ; edx = GB thứ mấy (0..3) chứa framebuffer

    mov edi, 0x4000
    cmp edx, 0
    je .fb_pd_selected
    mov edi, 0x11000
    cmp edx, 1
    je .fb_pd_selected
    mov edi, 0x12000
    cmp edx, 2
    je .fb_pd_selected
    mov edi, 0x13000
.fb_pd_selected:

    mov ecx, eax
    shr ecx, 21
    and ecx, 0x1FF               ; ecx = chỉ số entry (0..511) trong PD
    imul ecx, ecx, 8
    add edi, ecx                 ; edi = địa chỉ entry 2MB đầu tiên cần sửa

    ; Đánh dấu 16 entry liên tiếp (16 * 2MB = 32MB) là Write-Combining -
    ; đủ cho hầu hết độ phân giải VBE 32bpp thông dụng (vd 1920x1080x4
    ; ~= 8MB).
    mov ecx, 16
.fb_mark_wc_loop:
    mov edx, ebx
    or edx, 0x8B                 ; Present(1)+Write(2)+PWT(0x08)+PS(0x80) -> chon slot PAT WC
    mov [edi], edx
    add edi, 8
    add ebx, 0x200000
    loop .fb_mark_wc_loop

    ; Nạp lại CR3 để chắc chắn TLB không còn giữ entry cacheable cũ.
    mov eax, 0x1000
    mov cr3, eax

    popad
    ret

print32:
    push eax
    push ebx
    push edx
    push edi
    push esi
.loop:
    lodsb
    cmp al, 0
    je .done
    cmp al, 0x0A
    je .newline
    cmp al, 0x0D
    je .loop
    mov ah, 0x0F
    mov [edi], ax
    add edi, 2
    jmp .loop
.newline:
    mov eax, edi
    sub eax, 0xB8000
    mov ebx, 80*2
    xor edx, edx
    div ebx
    inc eax
    mul ebx
    add eax, 0xB8000
    mov edi, eax
    jmp .loop
.done:
    pop esi
    pop edi
    pop edx
    pop ebx
    pop eax
    ret

; ================================================
; 64-BIT LONG MODE
; ================================================
[BITS 64]
long_mode_start:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    ; 🔥 SỬA: cùng lý do như protected_start - chỉ in debug qua 0xB8000
    ; khi VBE thất bại, vì khi đó mới còn ở text mode thật.
    cmp byte [vbe_ok], 1
    je .skip_text_debug

    mov rsi, msg3
    mov rdi, 0xB8000 + 4*80*2
    call print64

    mov rsi, msg4
    mov rdi, 0xB8000 + 6*80*2
    call print64

    mov rsi, msg_vbe_info
    mov rdi, 0xB8000 + 8*80*2
    call print64

.skip_text_debug:

    ; 🔥 SỬA: XÓA đoạn copy "vbe_info" (buffer rỗng, không bao giờ được
    ; ghi dữ liệu thật) đè lên 0x6000 - nó sẽ xóa mất struct VBE thật
    ; vừa được ghi vào 0x6000 ở phần .continue_boot phía trên!

    jmp 0x20000

.hang:
    hlt
    jmp .hang

print64:
    push rax
    push rbx
    push rdx
    push rdi
    push rsi
.loop:
    lodsb
    cmp al, 0
    je .done
    cmp al, 0x0A
    je .newline
    cmp al, 0x0D
    je .loop
    mov ah, 0x0F
    mov [rdi], ax
    add rdi, 2
    jmp .loop
.newline:
    mov rax, rdi
    sub rax, 0xB8000
    mov rbx, 80*2
    xor rdx, rdx
    div rbx
    inc rax
    mul rbx
    add rax, 0xB8000
    mov rdi, rax
    jmp .loop
.done:
    pop rsi
    pop rdi
    pop rdx
    pop rbx
    pop rax
    ret

; ================================================
; MESSAGES 32/64
; ================================================
msg2 db "Protected Mode Completed!", 0x0D, 0x0A, 0
msg3 db "Long Mode Completed!", 0x0D, 0x0A, 0
msg4 db "Load Kernel Completed!", 0
msg_vbe_info db "VBE info passed to kernel", 0

; ================================================
; PAD TO 8192 BYTES
; ================================================
times 8192 - ($ - $$) db 0