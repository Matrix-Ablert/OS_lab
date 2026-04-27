org 0x7c00
[bits 16]

; --- 1. 一键清屏 ---
mov ax, 0x0003
int 0x10

xor ax, ax
mov ds, ax
mov ax, 0xb800
mov es, ax

; 出生点：屏幕正中央
mov word [x], 40
mov word [y], 12
mov byte [dir], 0 ; 0=发呆, 1=W(上), 2=A(左), 3=S(下), 4=D(右)

snake_loop:
    ; --- 2. 非阻塞读取键盘输入 ---
    mov ah, 01h
    int 16h
    jz update_pos ; 如果没按键，跳转去更新位置
    
    ; 如果有按键，读出按键并清空缓冲区
    mov ah, 00h
    int 16h
    cmp al, 'w'
    je set_w
    cmp al, 'a'
    je set_a
    cmp al, 's'
    je set_s
    cmp al, 'd'
    je set_d
    jmp update_pos

set_w: mov byte [dir], 1
       jmp update_pos
set_a: mov byte [dir], 2
       jmp update_pos
set_s: mov byte [dir], 3
       jmp update_pos
set_d: mov byte [dir], 4

update_pos:
    ; 如果 direction 为 0 (刚开机还没按键)，则原地画蛇，不移动
    cmp byte [dir], 0
    je draw_snake

    ; --- 3. 擦除蛇头旧位置 (替换为黑底空格) ---
    mov ax, [y]
    mov cx, 80
    mul cx
    add ax, [x]
    shl ax, 1
    mov di, ax
    mov word [es:di], 0x0720 

    ; --- 4. 根据方向计算新坐标 (带有穿墙逻辑) ---
    mov al, [dir]
    cmp al, 1
    je go_w
    cmp al, 2
    je go_a
    cmp al, 3
    je go_s
    cmp al, 4
    je go_d

go_w: 
    dec word [y]
    cmp word [y], 0
    jge draw_snake
    mov word [y], 24 ; 撞上边界，从下边界穿出
    jmp draw_snake
go_a: 
    dec word [x]
    cmp word [x], 0
    jge draw_snake
    mov word [x], 79 ; 穿出左边界
    jmp draw_snake
go_s: 
    inc word [y]
    cmp word [y], 24
    jle draw_snake
    mov word [y], 0  ; 穿出下边界
    jmp draw_snake
go_d: 
    inc word [x]
    cmp word [x], 79
    jle draw_snake
    mov word [x], 0  ; 穿出右边界

draw_snake:
    ; --- 5. 在新坐标画出蛇头 ---
    mov ax, [y]
    mov cx, 80
    mul cx
    add ax, [x]
    shl ax, 1
    mov di, ax
    mov word [es:di], 0x0A4F ; 0x0A = 亮绿色前景, 0x4F = 字符 'O'

    ; --- 6. 游戏帧率延时 (控制移动速度，0.08秒) ---
    mov ah, 0x86
    mov cx, 0x0001
    mov dx, 0x3880 
    int 0x15

    jmp snake_loop

; 数据段
x: dw 40
y: dw 12
dir: db 0

times 510 - ($ - $$) db 0
dw 0xAA55