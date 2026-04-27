org 0x7c00
[bits 16]

mov ax, 0x0003  ; 功能号 AH=00h (设置视频模式), AL=03h (80x25 16色文本模式)
int 0x10        ; 调用 BIOS 视频中断，这会瞬间清空屏幕上的所有历史残留

echo_loop:
    ; --- 1. 等待并读取键盘输入 ---
    mov ah, 0x00 
    int 0x16        ; 触发键盘中断。程序会阻塞在这里等敲击。按下后，AL = 字符的ASCII码

    ; --- 2. 将按下的字符回显到屏幕 ---
    ; 这里我们使用 10h 中断的 0x0E 功能号 (电传打字机模式/Teletype output)
    ; 它非常聪明，打印完字符后会自动把光标推到下一个位置，连回车换行都能自动处理！
    mov ah, 0x0E    
    mov bh, 0x00    
    mov bl, 0x07    ; 灰白色文本
    int 0x10

    jmp echo_loop   ; 无限循环，等待下一个键盘敲击

times 510-($-$$) db 0
db 0x55, 0xaa