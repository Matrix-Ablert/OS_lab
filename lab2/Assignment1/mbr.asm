org 0x7c00
[bits 16]

xor ax, ax
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax

; 初始化显存段寄存器 gs
mov ax, 0xb800
mov gs, ax

; 初始化栈指针
mov sp, 0x7c00

; --- 循环打印逻辑开始 ---
mov si, my_id       ; si 寄存器指向字符串首地址
mov di, 2580        ; di 寄存器记录显存偏移量 (第16行, 第10列)
mov ah, 0x2F        ; ah 存储颜色属性 (绿底白字)

print_loop:
    mov al, [si]    ; 从字符串中取出一个字符放到 al
    cmp al, 0       ; 检查是否遇到了字符串结尾的 0
    je print_end    ; 如果是 0，说明打印完毕，跳出循环

    mov [gs:di], ax ; 将 ah(颜色) 和 al(字符) 一起写入显存
    add si, 1       ; 字符串指针后移 1 个字节
    add di, 2       ; 显存指针后移 2 个字节 (因为每个字符占2字节)
    jmp print_loop  ; 继续下一次循环

print_end:
    jmp $           ; 死循环

; --- 数据定义部分 ---
my_id db '22347055', 0  ; 请在这里填入你真实的学号，末尾的 0 是结束标志

; --- 填充引导扇区 ---
times 510 - ($ - $$) db 0
db 0x55, 0xaa