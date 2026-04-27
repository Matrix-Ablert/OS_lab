; ================================================================
; Assignment 3: bootloader.asm  【修正版】
; 基于 Example 2 结构
; Bug修复:
;   1. [bits 16]段中不使用32位寄存器，避免操作数前缀导致指令错位
;   2. pgdt 紧跟在 lgdt 调用附近（[bits 16]段内），与 Example 2 一致
;   3. 实模式打印改为纯16位写法
; ================================================================

%include "boot.inc"
org 0x7e00
[bits 16]

; ----------------------------------------------------------------
; 实模式：打印启动提示（纯16位写法，避免32位前缀错位）
; ----------------------------------------------------------------
    mov ax, 0xb800
    mov gs, ax
    mov ah, 0x03                    ; 青色属性
    mov si, bootloader_tag          ; 纯16位 SI
    xor di, di                      ; 纯16位 DI
output_bootloader_tag:
    mov al, [si]
    cmp al, 0
    je  gdt_setup
    mov [gs:di], ax
    inc si
    add di, 2
    jmp output_bootloader_tag

; ----------------------------------------------------------------
; 构建 GDT（写入 0x8800，与 Example 2 完全一致）
; ----------------------------------------------------------------
gdt_setup:
    mov dword [GDT_START_ADDRESS+0x00], 0x00000000
    mov dword [GDT_START_ADDRESS+0x04], 0x00000000

    mov dword [GDT_START_ADDRESS+0x08], 0x0000ffff
    mov dword [GDT_START_ADDRESS+0x0c], 0x00cf9200

    mov dword [GDT_START_ADDRESS+0x10], 0x00000000
    mov dword [GDT_START_ADDRESS+0x14], 0x00409600

    mov dword [GDT_START_ADDRESS+0x18], 0x80007fff
    mov dword [GDT_START_ADDRESS+0x1c], 0x0040920b

    mov dword [GDT_START_ADDRESS+0x20], 0x0000ffff
    mov dword [GDT_START_ADDRESS+0x24], 0x00cf9800

; ----------------------------------------------------------------
; 加载 GDTR，开启 A20，进入保护模式
; ----------------------------------------------------------------
    mov word [pgdt], 39
    lgdt [pgdt]

    in  al, 0x92
    or  al, 0000_0010B
    out 0x92, al

    cli
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp dword CODE_SELECTOR:protect_mode_begin

; *** pgdt 必须在 [bits 16] 段内、jmp 之后 ***
pgdt:   dw 0
        dd GDT_START_ADDRESS

bootloader_tag  db 'run bootloader', 0

; ================================================================
; 32位保护模式
; ================================================================
[bits 32]
protect_mode_begin:

    mov eax, DATA_SELECTOR
    mov ds, eax
    mov es, eax
    mov eax, STACK_SELECTOR
    mov ss, eax
    mov eax, VIDEO_SELECTOR
    mov gs, eax

    mov esp, 0x9C000

    ; --------------------------------------------------------
    ; Task 3.2: hex_dump GDT（0x8800，40字节）
    ; --------------------------------------------------------
    call clear_screen

    mov  esi, msg_title
    mov  edx, 0
    call print_string_pm

    mov  esi, GDT_START_ADDRESS
    mov  ecx, 40
    mov  edx, 1
    call hex_dump

    mov  esi, msg_gdt_hint
    mov  edx, 5
    call print_string_pm

    ; 等待截图（约4秒）
    mov  ecx, 0x6FFFFFFF
.wait:
    dec  ecx
    jnz  .wait

    ; --------------------------------------------------------
    ; Task 3.1: 保护模式跑马灯
    ; --------------------------------------------------------
    call clear_screen

    mov  esi, center_text
    mov  edi, (12 * 80 + 32) * 2
    mov  ah,  0x0A
.draw_center:
    mov  al, [esi]
    cmp  al, 0
    je   .center_done
    mov  [gs:edi], ax
    inc  esi
    add  edi, 2
    jmp  .draw_center
.center_done:

    mov  dword [var_x],     0
    mov  dword [var_y],     0
    mov  byte  [var_dir],   0
    mov  byte  [var_color], 1
    mov  byte  [var_char],  '0'

main_loop:
    mov  eax, [var_y]
    mov  ecx, 80
    mul  ecx
    add  eax, [var_x]
    shl  eax, 1
    mov  edi, eax

    mov  al, [var_char]
    mov  ah, [var_color]
    mov  [gs:edi], ax

    call delay_pm

    inc  byte [var_color]
    inc  byte [var_char]
    cmp  byte [var_char], '9' + 1
    jne  .check_dir
    mov  byte [var_char], '0'

.check_dir:
    mov  al, [var_dir]
    cmp  al, 0
    je   .move_right
    cmp  al, 1
    je   .move_down
    cmp  al, 2
    je   .move_left
    jmp  .move_up

.move_right:
    inc  dword [var_x]
    cmp  dword [var_x], 79
    jl   .next_iter
    mov  byte  [var_dir], 1
    jmp  .next_iter

.move_down:
    inc  dword [var_y]
    cmp  dword [var_y], 24
    jl   .next_iter
    mov  byte  [var_dir], 2
    jmp  .next_iter

.move_left:
    dec  dword [var_x]
    cmp  dword [var_x], 0
    jg   .next_iter
    mov  byte  [var_dir], 3
    jmp  .next_iter

.move_up:
    dec  dword [var_y]
    cmp  dword [var_y], 0
    jg   .next_iter
    mov  byte  [var_dir], 0

.next_iter:
    jmp  main_loop

; ================================================================
; 子程序
; ================================================================

; 忙等待延时（替代保护模式下不可用的 int 0x15）
delay_pm:
    push ecx
    mov  ecx, 0x3FFFF
.lp:
    dec  ecx
    jnz  .lp
    pop  ecx
    ret

; 清屏（替代保护模式下不可用的 int 0x10）
clear_screen:
    push eax
    push ecx
    push edi
    xor  edi, edi
    mov  ecx, 80 * 25
    mov  ax,  0x0720
.cls_loop:
    mov  [gs:edi], ax
    add  edi, 2
    dec  ecx
    jnz  .cls_loop
    pop  edi
    pop  ecx
    pop  eax
    ret

; 打印字符串到指定行（ESI=字符串地址, EDX=行号）
print_string_pm:
    push eax
    push edi
    mov  eax, edx
    imul eax, 80 * 2
    mov  edi, eax
.loop:
    mov  al, [esi]
    cmp  al, 0
    je   .done
    mov  ah, 0x07
    mov  [gs:edi], ax
    inc  esi
    add  edi, 2
    jmp  .loop
.done:
    pop  edi
    pop  eax
    ret

; hex_dump（ESI=起始地址, ECX=字节数, EDX=起始行号）
hex_dump:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

.line_loop:
    cmp  ecx, 0
    je   .dump_done

    mov  eax, edx
    imul eax, 80 * 2
    mov  edi, eax

    ; 输出4位地址（黄色 0x0E）
    mov  eax, esi
    shr  eax, 12
    and  eax, 0xF
    mov  al,  [hex_table + eax]
    mov  ah,  0x0E
    mov  [gs:edi], ax
    add  edi, 2

    mov  eax, esi
    shr  eax, 8
    and  eax, 0xF
    mov  al,  [hex_table + eax]
    mov  ah,  0x0E
    mov  [gs:edi], ax
    add  edi, 2

    mov  eax, esi
    shr  eax, 4
    and  eax, 0xF
    mov  al,  [hex_table + eax]
    mov  ah,  0x0E
    mov  [gs:edi], ax
    add  edi, 2

    mov  eax, esi
    and  eax, 0xF
    mov  al,  [hex_table + eax]
    mov  ah,  0x0E
    mov  [gs:edi], ax
    add  edi, 2

    mov  word [gs:edi], 0x073A      ; ':'
    add  edi, 2
    mov  word [gs:edi], 0x0720      ; ' '
    add  edi, 2

    ; 本行最多16字节
    mov  ebx, 16
    cmp  ecx, 16
    jge  .byte_loop
    mov  ebx, ecx

.byte_loop:
    cmp  ebx, 0
    je   .line_done

    mov  al, [esi]

    ; 高4位
    mov  ah, al
    shr  ah, 4
    and  ah, 0x0F
    movzx eax, ah
    mov  al, [hex_table + eax]
    mov  ah, 0x07
    mov  [gs:edi], ax
    add  edi, 2

    ; 低4位
    mov  al, [esi]
    and  al, 0x0F
    movzx eax, al
    mov  al, [hex_table + eax]
    mov  ah, 0x07
    mov  [gs:edi], ax
    add  edi, 2

    mov  word [gs:edi], 0x0720
    add  edi, 2

    inc  esi
    dec  ebx
    dec  ecx
    jmp  .byte_loop

.line_done:
    inc  edx
    jmp  .line_loop

.dump_done:
    pop  edi
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    pop  eax
    ret

; ================================================================
; 数据区
; ================================================================
hex_table    db '0123456789ABCDEF'
msg_title    db '[Task 3.2] GDT Hex Dump  addr=0x8800  40 bytes', 0
msg_gdt_hint db 'Compare above with Assignment 2.2 manual parse ^', 0
center_text  db '22347055 Matrix', 0   ; ← 改为你的学号

var_x:       dd 0
var_y:       dd 0
var_dir:     db 0
var_color:   db 1
var_char:    db '0'
