org 0x7c00
[bits 16]

; --- 1. 一键清屏 ---
mov ax, 0x0003
int 0x10

; --- 2. 初始化寄存器 ---
xor ax, ax
mov ds, ax
mov ax, 0xb800
mov es, ax

; --- 3. 绘制中央常驻的黑客帝国风烙印 ---
mov si, center_text
mov di, 1984
mov ah, 0x0A         ; 黑底亮绿
print_center:
    mov al, [si]
    cmp al, 0
    je print_done
    mov [es:di], ax
    inc si
    add di, 2
    jmp print_center
print_done:

; --- 4. 初始化螺旋变量 ---
mov word [x], 0
mov word [y], 0
mov byte [dir], 0

mov word [min_x], 0
mov word [max_x], 79
mov word [min_y], 0
mov word [max_y], 24

mov byte [color], 1
mov byte [char], '0'

spiral_loop:
    ; --- 判断是否走到死胡同 ---
    mov ax, [min_x]
    cmp ax, [max_x]
    jg spiral_end
    mov ax, [min_y]
    cmp ax, [max_y]
    jg spiral_end

    ; --- 绘制螺旋轨迹（走到中心会自动覆盖绿字） ---
    mov ax, [y]
    mov cx, 80
    mul cx
    add ax, [x]
    shl ax, 1
    mov di, ax
    
    mov al, [char]
    mov ah, [color]
    mov [es:di], ax

    ; --- 延时 ---
    mov ah, 0x86
    mov cx, 0x0000
    mov dx, 0x86A0 
    int 0x15

    ; --- 变换颜色和字符 ---
    inc byte [color]
    inc byte [char]
    cmp byte [char], '9' + 1
    jne check_dir
    mov byte [char], '0'

check_dir:
    ; --- 螺旋移动逻辑与空气墙收缩 ---
    mov al, [dir]
    cmp al, 0
    je move_r
    cmp al, 1
    je move_d
    cmp al, 2
    je move_l
    cmp al, 3
    je move_u

move_r:
    mov ax, [x]
    cmp ax, [max_x]
    jl do_r
    mov byte [dir], 1
    inc word [min_y]
    jmp move_d
do_r:
    inc word [x]
    jmp next_sp

move_d:
    mov ax, [y]
    cmp ax, [max_y]
    jl do_d
    mov byte [dir], 2
    dec word [max_x]
    jmp move_l
do_d:
    inc word [y]
    jmp next_sp

move_l:
    mov ax, [x]
    cmp ax, [min_x]
    jg do_l
    mov byte [dir], 3
    dec word [max_y]
    jmp move_u
do_l:
    dec word [x]
    jmp next_sp

move_u:
    mov ax, [y]
    cmp ax, [min_y]
    jg do_u
    mov byte [dir], 0
    inc word [min_x]
    jmp move_r
do_u:
    dec word [y]

next_sp:
    jmp spiral_loop

spiral_end:
    jmp $ 

; 数据段
x: dw 0
y: dw 0
dir: db 0
min_x: dw 0
max_x: dw 79
min_y: dw 0
max_y: dw 24
color: db 1
char: db '0'
center_text: db '22347055 Matrix', 0 ; 专属烙印

times 510 - ($ - $$) db 0
dw 0xAA55