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

    mov si, msg1
    call print

    cmp dl, 0
    jne .drive_ok
    mov dl, 0x80

.drive_ok:
    mov [boot_drive], dl

    ; ---- Load kernel in multiple chunks (up to 256 sectors / 128KB) ----
    mov word [dap_count], 64
    mov word [dap_offset], 0x0000
    mov word [dap_segment], 0x2000
    mov dword [dap_lba_low], 17
    mov dword [dap_lba_high], 0

    mov cx, 4                  ; 4 chunks * 64 sectors = 256 sectors (128KB)
.load_loop:
    push cx
    mov di, 5
.retry_read:
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc .chunk_ok

    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    dec di
    jnz .retry_read
    jmp .disk_error

.chunk_ok:
    add dword [dap_lba_low], 64
    add word [dap_segment], 0x0800
    pop cx
    loop .load_loop

.read_success:
    mov si, msg_kernel_loaded
    call print

    cli
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_desc]

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

vbe_init:
    pusha
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov byte [vbe_ok], 0

    mov si, msg_vbe_check
    call print

    mov ax, 0x4F00
    mov di, 0x5000
    int 0x10
    
    cmp ax, 0x004F
    jne .failed
    
    cmp dword [0x5000], 'VESA'
    jne .failed

    mov si, msg_vbe_found
    call print

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

    mov ax, [0x5200]
    test ax, 1
    jz .next_mode_advance
    test ax, 16
    jz .next_mode_advance

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

    mov ax, 0x4F02
    mov bx, [vbe_mode]
    or bx, 0x4000
    int 0x10

    cmp ax, 0x004F
    jne .failed

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

bga_force_1280x720x32:
    pusha

    mov dx, 0x01CE
    mov ax, 4
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0
    out dx, ax

    mov dx, 0x01CE
    mov ax, 1
    out dx, ax
    mov dx, 0x01CF
    mov ax, 1280
    out dx, ax

    mov dx, 0x01CE
    mov ax, 2
    out dx, ax
    mov dx, 0x01CF
    mov ax, 720
    out dx, ax

    mov dx, 0x01CE
    mov ax, 3
    out dx, ax
    mov dx, 0x01CF
    mov ax, 32
    out dx, ax

    mov dx, 0x01CE
    mov ax, 4
    out dx, ax
    mov dx, 0x01CF
    mov ax, 0x41
    out dx, ax

    mov dword [boot_pitch], 1280*4
    mov word [boot_width], 1280
    mov word [boot_height], 720
    mov byte [boot_bpp], 32

    popa
    ret

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
    je .finish
    mov ah, 0x0E
    int 0x10
    jmp .loop
.finish:
    pop si
    pop ax
    ret

align 4
dap:
    db 0x10
    db 0
dap_count:
    dw 64
dap_offset:
    dw 0x0000
dap_segment:
    dw 0x2000
dap_lba_low:
    dd 17
dap_lba_high:
    dd 0

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

[BITS 32]
protected_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

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

    mov ecx, 0x277
    mov eax, 0x00070106
    mov edx, 0x00070406
    wrmsr

    mov edi, 0x1000
    xor eax, eax
    mov ecx, 0x2000 / 4
    rep stosd

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

    mov dword [0x1000], 0x2000 | 0x03

    mov dword [0x2000 + 0*8], 0x4000  | 0x03
    mov dword [0x2000 + 1*8], 0x11000 | 0x03
    mov dword [0x2000 + 2*8], 0x12000 | 0x03
    mov dword [0x2000 + 3*8], 0x13000 | 0x03

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

    mov eax, [boot_fb]
    and eax, 0xFFE00000
    mov ebx, eax

    mov edx, eax
    shr edx, 30
    and edx, 3

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
    and ecx, 0x1FF
    imul ecx, ecx, 8
    add edi, ecx

    mov ecx, 16
.fb_mark_wc_loop:
    mov edx, ebx
    or edx, 0x8B
    mov [edi], edx
    add edi, 8
    add ebx, 0x200000
    loop .fb_mark_wc_loop

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
    je .finish
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
.finish:
    pop esi
    pop edi
    pop edx
    pop ebx
    pop eax
    ret

[BITS 64]
long_mode_start:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

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
    je .finish
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
.finish:
    pop rsi
    pop rdi
    pop rdx
    pop rbx
    pop rax
    ret

msg2 db "Protected Mode Completed!", 0x0D, 0x0A, 0
msg3 db "Long Mode Completed!", 0x0D, 0x0A, 0
msg4 db "Load Kernel Completed!", 0
msg_vbe_info db "VBE info passed to kernel", 0

times 8192 - ($ - $$) db 0