org 0x7c00
[bits 16]

; --- 1. 获取当前光标位置 ---
mov ah, 0x03
mov bh, 0x00    ; 第0页
int 0x10        ; 调用中断后，DH保存当前行号，DL保存当前列号

; --- 2. 移动光标位置 ---
add dh, 5       ; 行号向下移 5 行
add dl, 5       ; 列号向右移 5 列
mov ah, 0x02
mov bh, 0x00
int 0x10        ; 再次调用中断，光标已经被设置到了新位置

; --- 3. 证明光标移过来了：在当前光标处写个 'X' ---
mov ah, 0x09
mov al, 'X'
mov bh, 0x00
mov bl, 0x0A    ; 亮绿色
mov cx, 1       ; 只写 1 个字符
int 0x10

jmp $           ; 死循环

times 510-($-$$) db 0
db 0x55, 0xaa