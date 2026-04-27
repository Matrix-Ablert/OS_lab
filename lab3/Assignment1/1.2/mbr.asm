org 0x7c00
[bits 16]

xor ax, ax
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax
mov gs, ax
mov sp, 0x7c00

mov ax, 1                ; 起始逻辑扇区号 LBA = 1
mov cx, 5                ; 需要加载的扇区数量 (循环5次)
mov bx, 0x7e00           ; bootloader的加载起始地址

load_bootloader:
    push cx
    call read_sector_chs ; 读取单个扇区
    inc ax               ; LBA++
    add bx, 512          ; 内存地址后移512字节
    pop cx
    loop load_bootloader

jmp 0x0000:0x7e00        ; 跳转到bootloader
jmp $ 

; ----------------------------------------------------
; read_sector_chs: 使用 int 13h 读取一个扇区
; 参数:
;   ax = LBA 逻辑扇区号
;   es:bx = 目标内存地址
; ----------------------------------------------------
read_sector_chs:
    pusha                ; 保护所有通用寄存器

    ; 1. 计算 S = (LBA % 63) + 1
    mov cl, 63           ; SPT = 63
    div cl               ; AL = LBA / 63 (商), AH = LBA % 63 (余数)
    mov cl, ah           
    inc cl               ; CL = S (扇区号)

    ; 2. 计算 C 和 H
    ; 此时 AL = LBA / 63, 我们需要再除以 HPC (18)
    mov ah, 0            ; 清空 AH 准备下一次除法
    mov dl, 18           ; HPC = 18
    div dl               ; AL = (LBA/63) / 18 -> C (柱面号)
                         ; AH = (LBA/63) % 18 -> H (磁头号)
    mov ch, al           ; CH = C
    mov dh, ah           ; DH = H

    ; 3. 调用 int 13h
    mov dl, 0x80         ; 驱动器号 80h (第一块硬盘)
    mov ah, 0x02         ; 功能号 02h: 读扇区
    mov al, 0x01         ; 读 1 个扇区
                         ; BX 已经是要写入的内存偏移地址
    int 0x13             ; 调用 BIOS 中断

    popa                 ; 恢复寄存器
    ret

times 510 - ($ - $$) db 0
db 0x55, 0xaa