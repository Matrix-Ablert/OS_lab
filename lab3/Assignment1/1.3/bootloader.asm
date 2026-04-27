org 0x7e00
[bits 16]

; 打印 "run bootloader"
mov ax, 0xb800
mov gs, ax
mov ah, 0x03             ; 青色
mov ecx, bootloader_tag_end - bootloader_tag
mov ebx, 80 * 2          ; 【修改】从第 2 行开始打印，避免覆盖第一行的 BOOT OK
mov esi, bootloader_tag

output_tag:
    mov al, [esi]
    mov word [gs:ebx], ax
    inc esi
    add ebx, 2
    loop output_tag

jmp $                    ; 死循环

bootloader_tag db 'run bootloader'
bootloader_tag_end:

; 【关键：填充与魔数】
; $ 是当前地址，$$ 是段起始地址 (0x7e00)
; 我们要让这个文件编译出来正好是 5 个扇区 (2560 字节)
; 所以填充 (2560 - 4字节魔数 - 当前已占用字节) 个 0
times 2560 - 4 - ($ - $$) db 0
dd 0xCAFEBABE            ; 这就是 4 字节的魔数 (Define Doubleword)