org 0x7c00
[bits 16]

; 初始坐标设置：第 16 行 (DH=16)，第 10 列 (DL=10)
mov dh, 16
mov dl, 10
mov si, my_id    ; si 指向学号字符串首地址

print_loop:
    mov al, [si] ; 从字符串中取出一个字符
    cmp al, 0    ; 检查是否遇到结束符
    je end_print

    ; 步骤 1：使用中断将光标移动到目标坐标 (DH, DL)
    mov ah, 0x02
    mov bh, 0x00
    int 0x10

    ; 步骤 2：在当前光标位置打印该字符
    mov ah, 0x09
    ; al 里面已经是刚才取出的字符了
    mov bh, 0x00
    mov bl, 0x2F ; 绿底白字
    mov cx, 1    ; 输出 1 次
    int 0x10

    ; 步骤 3：更新指针和坐标，准备下一次循环
    inc dl       ; 列坐标向右移1位
    inc si       ; 字符串指针往后移1位
    jmp print_loop

end_print:
    jmp $

; 数据段
my_id db '22347055', 0  ; 

times 510-($-$$) db 0
db 0x55, 0xaa