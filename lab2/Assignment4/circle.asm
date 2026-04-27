org 0x7c00
[bits 16]

; --- 1. 一键清屏 ---
mov ax, 0x0003
int 0x10

; --- 2. 初始化段寄存器和显存基址 ---
xor ax, ax
mov ds, ax
mov ax, 0xb800
mov es, ax

; --- 3. 绘制中央常驻的黑客帝国风烙印 ---
mov si, center_text
mov di, 1984         ; 第12行，第32列的显存偏移量
mov ah, 0x0A         ; 颜色属性：0x0A (黑底亮绿，纯正Matrix风)
print_center:
    mov al, [si]
    cmp al, 0
    je print_done
    mov [es:di], ax  ; 写入显存
    inc si
    add di, 2
    jmp print_center
print_done:

; --- 4. 初始化跑马灯状态变量 ---
mov word [x], 0
mov word [y], 0
mov byte [dir], 0    
mov byte [color], 1  
mov byte [char], '0' 

main_loop:
    ; --- 绘制边缘跑马灯 ---
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
    ; --- 边缘移动逻辑 ---
    mov al, [dir]
    cmp al, 0
    je move_right
    cmp al, 1
    je move_down
    cmp al, 2
    je move_left
    cmp al, 3
    je move_up

move_right:
    inc word [x]
    cmp word [x], 79
    jl next_iter
    mov byte [dir], 1    
    jmp next_iter
move_down:
    inc word [y]
    cmp word [y], 24
    jl next_iter
    mov byte [dir], 2    
    jmp next_iter
move_left:
    dec word [x]
    cmp word [x], 0
    jg next_iter
    mov byte [dir], 3    
    jmp next_iter
move_up:
    dec word [y]
    cmp word [y], 0
    jg next_iter
    mov byte [dir], 0    

next_iter:
    jmp main_loop

; --- 数据段 ---
x: dw 0
y: dw 0
dir: db 0
color: db 1
char: db '0'
center_text: db '22347055 Matrix', 0  ; 你的专属烙印

times 510 - ($ - $$) db 0
dw 0xAA55