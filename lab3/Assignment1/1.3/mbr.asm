org 0x7c00
[bits 16]

xor ax, ax
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax
mov gs, ax
mov sp, 0x7c00

mov ax, 1
mov cx, 5
mov bx, 0x7e00

load_bootloader:
    push cx
    call read_sector_chs 
    inc ax
    add bx, 512
    pop cx
    loop load_bootloader

; ----------------------------------------------------
; 校验魔数
; ----------------------------------------------------
    mov eax, dword [0x87FC]       ; 获取 bootloader 尾部的 4 字节
    cmp eax, 0xCAFEBABE           ; 比较魔数
    je boot_success               ; 如果相等，跳转到成功逻辑

boot_fail:
    ; 打印 BOOT ERR
    mov ax, 0xb800
    mov gs, ax
    mov ah, 0x04                  ; 红色
    mov esi, msg_err
    xor ebx, ebx
.print_err:
    mov al, [esi]
    cmp al, 0
    je .halt_system
    mov word [gs:bx], ax
    inc esi
    add ebx, 2
    jmp .print_err
.halt_system:
    hlt
    jmp .halt_system              ; 死机

boot_success:
    ; 打印 BOOT OK
    mov ax, 0xb800
    mov gs, ax
    mov ah, 0x0A                  ; 亮绿色
    mov esi, msg_ok
    xor ebx, ebx
.print_ok:
    mov al, [esi]
    cmp al, 0
    je .do_jump
    mov word [gs:bx], ax
    inc esi
    add ebx, 2
    jmp .print_ok
.do_jump:
    jmp 0x0000:0x7e00             ; 成功，跳转到 Bootloader

; ----------------------------------------------------
msg_ok  db 'BOOT OK', 0
msg_err db 'BOOT ERR', 0

; read_sector_chs 子程序 (同 1.2 的实现)
read_sector_chs:
    pusha
    mov cl, 63
    div cl
    mov cl, ah
    inc cl
    mov ah, 0
    mov dl, 18
    div dl
    mov ch, al
    mov dh, ah
    mov dl, 0x80
    mov ah, 0x02
    mov al, 0x01
    int 0x13
    popa
    ret

times 510 - ($ - $$) db 0
db 0x55, 0xaa