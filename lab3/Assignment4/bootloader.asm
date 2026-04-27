; ================================================================
; Assignment 4: 保护模式简易内存浏览器
; 在 Assignment 3 基础上新增：
;   - 交互式键盘控制（直接读 I/O 端口 0x64/0x60）
;   - ↓/j：向后翻页（+256字节）
;   - ↑/k：向前翻页（-256字节）
;   - q：停机退出（hlt）
; ================================================================

%include "boot.inc"
org 0x7e00
[bits 16]

; ----------------------------------------------------------------
; 实模式：打印启动提示（纯16位）
; ----------------------------------------------------------------
    mov ax, 0xb800
    mov gs, ax
    mov ah, 0x03
    mov si, bootloader_tag
    xor di, di
output_bootloader_tag:
    mov al, [si]
    cmp al, 0
    je  gdt_setup
    mov [gs:di], ax
    inc si
    add di, 2
    jmp output_bootloader_tag

; ----------------------------------------------------------------
; 构建 GDT（与 Example 2 / Assignment 3 完全一致）
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

pgdt:           dw 0
                dd GDT_START_ADDRESS
bootloader_tag: db 'run bootloader', 0

; ================================================================
; 32位保护模式入口
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

    ; 初始浏览地址：从 0x7c00（MBR）开始
    mov dword [cur_addr], 0x7c00

; ================================================================
; 主循环：清屏 → 绘制界面 → 等待按键 → 更新地址
; ================================================================
browser_loop:

    call clear_screen

    ; --- 第0行：标题栏 ---
    mov  esi, msg_title
    mov  edx, 0
    call print_string_pm

    ; --- 第0行末尾：显示当前地址（8位十六进制）---
    mov  eax, [cur_addr]
    mov  edi, (0 * 80 + 22) * 2    ; 第0行第22列
    call print_hex32_at

    ; --- 第1行：操作提示 ---
    mov  esi, msg_hint
    mov  edx, 1
    call print_string_pm

    ; --- 第2行起：hex dump 当前页（256字节 = 16行）---
    mov  esi, [cur_addr]
    mov  ecx, 256
    mov  edx, 2
    call hex_dump

    ; --- 等待有效按键并处理 ---
    call wait_keypress

    jmp  browser_loop               ; 重绘

; ================================================================
; 子程序：wait_keypress
; 功能：循环读取键盘扫描码，响应有效按键后返回
;
; 键盘 I/O 原理（保护模式下替代 int 16h）：
;   - 端口 0x64 bit0 = 1：键盘输出缓冲区有数据
;   - 端口 0x60：读取扫描码
;   - make code（按下）：0x01~0x7F
;   - break code（松开）：make code | 0x80
;   - 只处理 make code，忽略 break code（避免一次按键触发多次）
; ================================================================
wait_keypress:
    push eax

.poll:
    in   al, 0x64               ; 读键盘状态寄存器
    test al, 0x01               ; bit0=1 表示有数据
    jz   .poll                  ; 无数据则继续等待

    in   al, 0x60               ; 读扫描码

    ; 忽略 break code（松开事件，bit7=1）
    test al, 0x80
    jnz  .poll

    ; 响应 make code
    cmp  al, 0x50               ; ↓ 按下
    je   .next_page
    cmp  al, 0x24               ; j 按下
    je   .next_page

    cmp  al, 0x48               ; ↑ 按下
    je   .prev_page
    cmp  al, 0x25               ; k 按下
    je   .prev_page

    cmp  al, 0x10               ; q 按下 → 退出
    je   .quit

    jmp  .poll                  ; 其他键忽略，继续等待

.next_page:
    add  dword [cur_addr], 256  ; 地址 +256
    jmp  .done

.prev_page:
    ; 防止地址下溢到负数
    cmp  dword [cur_addr], 256
    jl   .done                  ; 已在最低处，不再减
    sub  dword [cur_addr], 256  ; 地址 -256
    jmp  .done

.quit:
    ; 停机退出：关中断后执行 hlt
    call clear_screen
    mov  esi, msg_quit
    mov  edx, 12
    call print_string_pm
    cli
    hlt                         ; CPU 停机

.done:
    pop  eax
    ret

; ================================================================
; 子程序：print_hex32_at
; 功能：将 EAX 以8位十六进制显示在 EDI 指定的显存位置
; 输入：EAX = 要显示的32位值
;       EDI = 显存偏移（相对视频段基址）
; 颜色：青色 0x03
; ================================================================
print_hex32_at:
    push eax
    push ebx
    push ecx
    push edi

    ; 前缀 "0x"
    mov  word [gs:edi], 0x0330  ; '0' 青色
    add  edi, 2
    mov  word [gs:edi], 0x0378  ; 'x' 青色
    add  edi, 2

    mov  ecx, 8                 ; 8个 nibble
.loop:
    rol  eax, 4                 ; 从最高 nibble 开始旋转
    mov  ebx, eax
    and  ebx, 0x0F
    mov  bl,  [hex_table + ebx]
    mov  bh,  0x03              ; 青色
    mov  [gs:edi], bx
    add  edi, 2
    dec  ecx
    jnz  .loop

    pop  edi
    pop  ecx
    pop  ebx
    pop  eax
    ret

; ================================================================
; 子程序：clear_screen
; ================================================================
clear_screen:
    push eax
    push ecx
    push edi
    xor  edi, edi
    mov  ecx, 80 * 25
    mov  ax,  0x0720
.loop:
    mov  [gs:edi], ax
    add  edi, 2
    dec  ecx
    jnz  .loop
    pop  edi
    pop  ecx
    pop  eax
    ret

; ================================================================
; 子程序：print_string_pm
; 输入：ESI = 字符串地址（以0结尾），EDX = 行号
; ================================================================
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

; ================================================================
; 子程序：hex_dump
; 输入：ESI = 起始地址，ECX = 字节数，EDX = 起始行号
; 格式：XXXX: XX XX ... XX（每行16字节）
;       地址黄色(0x0E)，数据白色(0x07)
; ================================================================
hex_dump:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

.line_loop:
    cmp  ecx, 0
    je   .done

    ; 行首显存偏移
    mov  eax, edx
    imul eax, 80 * 2
    mov  edi, eax

    ; 输出4位地址（黄色）
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

    ; ': '
    mov  word [gs:edi], 0x073A
    add  edi, 2
    mov  word [gs:edi], 0x0720
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

    ; 空格
    mov  word [gs:edi], 0x0720
    add  edi, 2

    inc  esi
    dec  ebx
    dec  ecx
    jmp  .byte_loop

.line_done:
    inc  edx
    jmp  .line_loop

.done:
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
hex_table   db '0123456789ABCDEF'

msg_title   db 'Memory Browser  Addr:', 0
msg_hint    db '[Up/k] Prev page  [Down/j] Next page  [q] Quit', 0
msg_quit    db '  System halted. Press reset to restart.', 0

cur_addr:   dd 0x7c00           ; 当前浏览地址，初始为 MBR
