%include "boot.inc"
org 0x7e00
[bits 16]

; --- 1. 打印 "run bootloader" (实模式下) ---
mov ax, 0xb800
mov gs, ax
mov ah, 0x03 ; 青色
mov ecx, bootloader_tag_end - bootloader_tag
xor ebx, ebx
mov esi, bootloader_tag
output_bootloader_tag:
    mov al, [esi]
    mov word[gs:bx], ax
    inc esi
    add ebx, 2
    loop output_bootloader_tag


; --- 2. 准备 GDT ---
; 空描述符 (Offset 0x00)
mov dword [GDT_START_ADDRESS+0x00], 0x00
mov dword [GDT_START_ADDRESS+0x04], 0x00  

; 数据段描述符 0~4GB (Offset 0x08)
mov dword [GDT_START_ADDRESS+0x08], 0x0000ffff
mov dword [GDT_START_ADDRESS+0x0c], 0x00cf9200

; 堆栈段描述符 (Offset 0x10)
mov dword [GDT_START_ADDRESS+0x10], 0x00000000 
mov dword [GDT_START_ADDRESS+0x14], 0x00409600

; 显存描述符 (Offset 0x18)
mov dword [GDT_START_ADDRESS+0x18], 0x80007fff
mov dword [GDT_START_ADDRESS+0x1c], 0x0040920b

; 代码段描述符 0~4GB (Offset 0x20)
mov dword [GDT_START_ADDRESS+0x20], 0x0000ffff
mov dword [GDT_START_ADDRESS+0x24], 0x00cf9800

; 【新增】第6个描述符：自定义数据段 (Offset 0x28)
; 基地址=0x7000, 界限=0x1FFF, 粒度=字节, DPL=0
mov dword [GDT_START_ADDRESS+0x28], 0x70001fff
mov dword [GDT_START_ADDRESS+0x2c], 0x00409200


; --- 3. 初始化 GDTR ---
mov word [pgdt], 47      ; 修改描述符表的界限: 6个描述符 * 8字节 - 1 = 47
lgdt [pgdt]
      
; --- 4. 打开 A20 ---
in al, 0x92
or al, 0000_0010B
out 0x92, al

; --- 5. 设置 CR0 的 PE 位 ---
cli
mov eax, cr0
or eax, 1
mov cr0, eax
      
; --- 6. 远跳转进入保护模式 ---
jmp dword CODE_SELECTOR:protect_mode_begin


; ==========================================
; 进入 32 位保护模式
; ==========================================
[bits 32]           
protect_mode_begin:                              
    ; 加载标准段选择子
    mov eax, DATA_SELECTOR
    mov ds, eax
    mov es, eax
    mov eax, STACK_SELECTOR
    mov ss, eax
    mov eax, VIDEO_SELECTOR
    mov gs, eax

    ; --- 打印 "enter protect mode" ---
    mov ecx, protect_mode_tag_end - protect_mode_tag
    mov ebx, 80 * 2                 ; 显存第2行显示
    mov esi, protect_mode_tag
    mov ah, 0x0A                    ; 亮绿色
output_protect_mode_tag:
    mov al, [esi]
    mov word[gs:ebx], ax
    add ebx, 2
    inc esi
    loop output_protect_mode_tag


    ; ==========================================
    ; Assignment 2.3: 自定义段描述符验证逻辑
    ; ==========================================
    
    ; 1. 将我们新增的段选择子 (0x28) 加载到 FS 寄存器
    mov ax, 0x28
    mov fs, ax

    ; 2. 通过 FS 段寄存器，向偏移地址 0 处写入学号
    ; 这里我们用循环逐字节写入，防止大小端带来的倒序问题
    mov esi, my_student_id
    mov edi, 0             ; FS 的偏移地址从 0 开始
    mov ecx, 8             ; 学号有 8 个字符
.write_to_fs:
    mov al, [esi]
    mov [fs:edi], al       ; 写入物理地址 0x7000 + edi
    inc esi
    inc edi
    loop .write_to_fs

    ; 3. 通过 DS 段寄存器，从物理地址 0x7000 处读取刚才写入的内容，并输出到显存
    ; DS 是平坦模式(基址为0)，所以要访问刚才的数据，偏移地址必须是 0x7000
    mov esi, 0x7000        ; DS 的偏移地址
    mov edi, 80 * 2 * 3    ; 显存第4行开始显示
    mov ecx, 8             ; 读取 8 个字符
    mov ah, 0x0E           ; 黄色字体
.read_from_ds:
    mov al, [ds:esi]       ; 从平坦模式读取
    mov word [gs:edi], ax  ; 写入视频段 (打印到屏幕)
    inc esi
    add edi, 2
    loop .read_from_ds

    ; ==========================================
    
    jmp $ ; 死循环挂起

; ------------------------------------------
; 数据区
; ------------------------------------------
pgdt dw 0
     dd GDT_START_ADDRESS

bootloader_tag       db 'run bootloader'
bootloader_tag_end:

protect_mode_tag     db 'enter protect mode'
protect_mode_tag_end:

my_student_id        db '22347055' ; 你的学号

; 填充 Bootloader 补足 5 个扇区 (为了和 MBR 的读取代码兼容，非常关键！)
times 2560 - ($ - $$) db 0