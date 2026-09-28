org 0x7c00
_start:
	cli
	xor ax,ax
	mov ds, ax 
	mov es, ax
	mov ss, ax
	mov sp, 0x7c00
	sti
	
	mov ah, 0x00
	mov dl, 0x80 
	int 0x13 
	
    mov bx, 0x8000
	mov ah, 0x02
	mov al, 16     	; so sector
	mov ch, 0
	mov cl, 2
	mov dh, 0
	mov dl, 0x80
	int 0x13
	jc errors
	
	mov ah, 0x0E
	mov al, 'V'
	int 0x10
	
	jmp 0x0000:0x8000
errors:
	mov ah, 0x0E
	mov al, 'E'
	int 0x10
	jmp $
.loop:
	hlt
	jmp .loop
times 510 - ($ - $$) db 0
dw 0xAA55