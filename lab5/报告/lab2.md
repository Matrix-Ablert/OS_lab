# <center>Lab2 实验入门</center>

**本次实验部分代码和注释参考自大模型，其中Assignment4中的代码由大模型完成。**

> 实验环境：Ubuntu22.04

## Assignment1 MBR

### 1.1 复现Example1 

> mbr_hello.asm

```shell
nasm -f bin mbr.asm -o mbr.bin #  使用nasm将汇编代码编译为纯二进制的机器码

qemu-img create hd.img 10m # 创建一个10MB的虚拟硬盘文件

dd if=mbr.bin of=hd.img bs=512 count=1 seek=0 conv=notrunc # 使用dd命令将编译好的mbr.bin写入硬盘的第0个扇区(MBR)
```

![image-20260329214618767](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260329214618767.png)

> 本次实验中的编译命令均如上，只需修改命令中的文件名即可，后续报告中省略命令



启动qemu

````shell
qemu-system-i386 -hda hd.img -serial null -parallel stdio
````

![image-20260330081411949](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330081411949.png)

可以看到屏幕中输出了蓝色的`Hello World`。



### 1.2 自定义输出

> mbr_id.asm

`显存起始位置=0xB8000+2⋅(80⋅x+y)，其中 (x,y) 表示第 x 行第 y 列。`计算2 * (80 * 16 + 10) = 2580，得到学号的起始位置是2580，之后每个字符的偏移量是两个字节。字符的颜色属性的字节高4位表示背景色，低4位表示前景色，这里使用0x2F表示绿色，高亮白色。之后在对应位置输出字符时将低位修改为对应学号即可。

```assembly
org 0x7c00
[bits 16]
xor ax, ax ; eax = 0
; 初始化段寄存器, 段地址全部设为0
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax
mov gs, ax

; 初始化栈指针
mov sp, 0x7c00
mov ax, 0xb800
mov gs, ax


mov ah, 0x2F ; 绿色(2) 高亮白色(F)
mov al, '2' ; 2 * (80 * 16 + 10) = 2580
mov [gs:2580 + 2 * 0], ax

mov al, '2'
mov [gs:2580 + 2 * 1], ax

mov al, '3'
mov [gs:2580 + 2 * 2], ax

mov al, '4'
mov [gs:2580 + 2 * 3], ax

mov al, '7'
mov [gs:2580 + 2 * 4], ax

mov al, '0'
mov [gs:2580 + 2 * 5], ax

mov al, '5'
mov [gs:2580 + 2 * 6], ax

mov al, '5'
mov [gs:2580 + 2 * 7], ax




jmp $ ; 死循环

times 510 - ($ - $$) db 0
db 0x55, 0xaa
```

![image-20260330082312978](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330082312978.png)



启动QEMU

![image-20260330082516481](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330082516481.png)

可以看到成功在屏幕上的(16,10)位置处输出了学号。



### 1.3 循环输出

> mbr_loop.asm

使用di寄存器记录偏移量其初始值为2580，使用ax寄存器记录显示段起始地址0xb800，使用db寄存器存储学号信息，寄存器si存储字符串的首地址。每次循环时，从si寄存器的地址中取出字符作为ax寄存器的低8位，之后比较是否是结尾字符，若不是，输出字符然后si指针向后移动一位，di向后移动两位。如果是结尾字符，则跳出循环。

```assembly
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

; 循环打印逻辑
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

; 数据定义部分
my_id db '22347055', 0  ; 末尾的 0 是结束标志

times 510 - ($ - $$) db 0
db 0x55, 0xaa
```



![image-20260330083011993](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330083011993.png)

成功使用循环输出在屏幕上输出了学号。

## Assignment2 实模式中断

### 2.1 利用中断实现光标的位置获取和光标的移动

> cursor.asm

将0x03存入AH寄存器，这是BIOS中断0x10的“读取光标位置和形状”的功能号，将0x00存入BH寄存器，指定显示页码是第0页。调用BIOS 0x10视频服务中断，启用后BIOS会将光标行号存入DH，列号存入DL。

之后将DH DL寄存器中的值自增5(向下平移五行，向右平移五列)，将0x02存入AH寄存器，这是BIOS中断0x10的设置光标位置的功能号，将0x00存入BH寄存器，指定显示页码是第0页。调用BIOS 0x10视频服务中断，启用后BIOS会将光标移动到（dh,dl）处。

最后将0x09存入AH寄存器，表示在光标位置写入字符，低8位写入待输出字符，ebx寄存器中写入输出格式0x0A 和 输出页码 0x00，向cx寄存器中写入重复次数，最后启用中断向光标处输出’X‘。



```assembly
org 0x7c00
[bits 16]

; 获取当前光标位置
mov ah, 0x03	; 读取光标位置
mov bh, 0x00    ; 第0页
int 0x10        ; 调用中断后，DH保存当前行号，DL保存当前列号

; 移动光标位置
add dh, 5       ; 行号向下移 5 行
add dl, 5       ; 列号向右移 5 列
mov ah, 0x02	; 设置光标位置
mov bh, 0x00
int 0x10        ; 再次调用中断，光标已经被设置到了新位置

; 证明光标移动
mov ah, 0x09	; 在光标位置写入字符
mov al, 'X'		; 将字符X存入低8位
mov bh, 0x00
mov bl, 0x0A    ; 亮绿色
mov cx, 1       ; 只写 1 个字符
int 0x10		; 指定重复次数为一次

jmp $           ; 死循环

times 510-($-$$) db 0
db 0x55, 0xaa
```



![image-20260330084344849](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330084344849.png)

可以看到屏幕上在光标移动后的位置上输出了指定字符。



### 2.2 使用实模式下的中断来输出学号

> id_int.asm

思路同1.3，每次读取出一个字符然后比较是否是结尾字符。若不是，则使用中断设置光标位置，然后按照格式打印字符，之后将指向字符串的指针和列坐标均向后自增一位。若是结尾字符，则跳转。

```assembly
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

    ; 使用中断将光标移动到目标坐标 (DH, DL)
    mov ah, 0x02
    mov bh, 0x00
    int 0x10

    ; 在当前光标位置打印该字符
    mov ah, 0x09
    ; al 里面已经是刚才取出的字符了
    mov bh, 0x00
    mov bl, 0x2F ; 绿底白字
    mov cx, 1    ; 输出 1 次
    int 0x10

    ; 更新指针和坐标，准备下一次循环
    inc dl       ; 列坐标向右移1位
    inc si       ; 字符串指针往后移1位
    jmp print_loop

end_print:
    jmp $

; 数据段
my_id db '22347055', 0  ; 

times 510-($-$$) db 0
db 0x55, 0xaa
```



![image-20260330090148780](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330090148780.png)



### 2.3 利用键盘中断实现键盘输入并回显

> echo.asm

将0x00装入ah寄存器，BIOS的键盘中断服务功能号，表示等待按键并读取，0x16是软中断代码表示键盘服务中断，进程阻塞等待键盘IO，直到发生键盘中断，之后AL寄存器保存按键的ASCII码，AH保存硬件扫描码。

之后将AL保存的字符输出到屏幕，方式同上。

函数运行结束后跳转到开头，等待下一个键盘中断。

```assembly
org 0x7c00
[bits 16]

echo_loop:
    ; 等待并读取键盘输入
    mov ah, 0x00 	; 等待按键并读取
    int 0x16        ; 触发键盘中断。程序会阻塞在这里等敲击。按下后，AL = 字符的ASCII码

    ; 将按下的字符回显到屏幕
    mov ah, 0x0E    ; 使用 10h 中断的 0x0E 功能号 (电传打字机模式/Teletype output)
    mov bh, 0x00    
    mov bl, 0x07    ; 灰白色文本
    int 0x10		; 软中断指令：触发 BIOS 0x10 视频服务中断。BIOS 读取 AL 中的 ASCII 码，将其写入当前光标位置的显存，并自动递增光标的硬件坐标指针。

    jmp echo_loop   ; 无限循环，等待下一个键盘敲击

times 510-($-$$) db 0
db 0x55, 0xaa
```



![image-20260330090516310](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330090516310.png)



#### 实验改进：

在代码开头增加一个BIOS视频中断，这样在代码执行时会清空屏幕上的qemu的启动信息，从而得到更好的显示效果。

```assembly
mov ax, 0x0003  ; 功能号 AH=00h (设置视频模式), AL=03h (80x25 16色文本模式)
int 0x10        ; 调用 BIOS 视频中断，这会瞬间清空屏幕上的所有历史残留
```

![image-20260330202423293](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330202423293.png)





## Assignment3 汇编

> student.asm

### 3.1 分支逻辑

需要说明的是进行除法时被除数是64位，其中高32位是EDX寄存器，低32位是EAX寄存器，在进行除法运算前需要使cdq命令，将EAX 中的 32 位有符号数扩展为 EDX:EAX 构成的 64 位有符号数，之后在进行除法运算，其中EAX寄存器存储结果的商，EDX存储结果的余数。

```assembly
your_if:

    ; if a1 >= 40
    mov eax, [a1]
    cmp eax, 40
    jl .check_18            ; 如果小于 40，跳去检查 18
    
    ; if_flag = (a1 + 3) / 5
    add eax, 3
    mov ecx, 5
    cdq                     ; 用 cdq 扩展符号
    idiv ecx                
    mov [if_flag], eax
    jmp .end_if

.check_18:

    ; else if a1 >= 18
    cmp eax, 18
    jl .do_else             ; 如果小于 18，跳去执行 else
   
    ; if_flag = 80 - (a1 * 2)
    imul eax, 2            
    mov ecx, 80
    sub ecx, eax            
    mov [if_flag], ecx
    jmp .end_if

.do_else:

    ; else: if_flag = a1 << 5
    shl eax, 5              
    mov [if_flag], eax

.end_if:
```



### 3.2 循环逻辑

实现如下：

```assembly
your_while:

.while_start:
    mov eax, [a2]
    cmp eax, 25
    jge .while_end          ; 如果 a2 >= 25，跳出循环

    ; call my_random
    call my_random          ; 返回的随机字符保存在 AL 中
    
    ; while_flag[a2 * 2] = eax
    mov ecx, [a2]          
    shl ecx, 1              ; ecx = a2 * 2 (计算数组索引)

    ; 解引用指针并按单字节写入
    mov edi, [while_flag]   ; 把 while_flag 指向的真实堆地址取到 edi
    mov [edi + ecx], al     ; 将 AL (1字节) 写入 while_flag[a2 * 2]

    ; ++a2
    inc dword [a2]
    jmp .while_start        

.while_end:
```



### 3.3 函数实现

实现如下：

```assembly
your_function:

    ; for i = 0; your_string[i] != '\0'; ++i
    mov esi, 0              ; i = 0  

    ; 获取字符串指针的真实地址
    mov edi, [your_string]  ; 把 your_string 指向的地址取到 edi
.func_loop:

    ; 取出 your_string[i]
    movzx eax, byte [edi + esi]
    cmp al, 0               ; 遇到 '\0' 结束
    je .func_end            
    pushad					; 将8个通用寄存器的值依次压入堆栈，保存当前CPU上下文
    
    ; push (your_string[i] + 9) to stack
    add eax, 9  ; 将ASCII值增加9            
    push eax	; 将修改后的字符压入栈，作为参数传递给函数
        
    call print_a_char       
    
    add esp, 4   ; 栈指针增加4覆盖刚刚压入栈中的eax
    
    popad       ; 从堆栈中恢复压栈的8个通用寄存器的值            

    inc esi                 ; ++i
    jmp .func_loop          

.func_end:
    ret     
```



![image-20260330093041901](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330093041901.png)

代码成功通过了测试。



## Assignment4

### 4.1 字符回旋程序

> circle.asm

实现思路：在主循环中，先进行寻址计算，得到当前一维地址，然后输出字符。执行阻塞，主动挂起执行流，以控制单次执行耗时。之后数据变量自增，先对[color]和[char]进行自增操作，之后使用比较和跳转指令来控制字符的值在'1'~'9'之间。最后读取方向变量[dir]，利用比较和跳转逻辑对x或y的值进行修改，如果坐标与预设的边界值发生碰撞，坐标跳出界面，则覆写[dir]变量，最后通过无条件跳转返回main_loop。

```assembly
main_loop:
    ; 动态计算偏移并绘制字符
    mov ax, [y]         ; 从内存加载行坐标至 AX。
    mov cx, 80          ; 加载行跨度常量 80 至 CX。
    mul cx              ; 无符号乘法：DX:AX = AX * CX。计算基准行偏移。
    add ax, [x]         ; 加法指令：累加列坐标，得到逻辑一维偏移。
    shl ax, 1           ; 逻辑左移 1 位：等效于乘 2，转换为物理字节偏移。
    mov di, ax          ; 将物理偏移量转存至 DI。

    mov al, [char]      ; 加载当前字符至 AL。
    mov ah, [color]     ; 加载当前颜色属性至 AH。
    mov [es:di], ax     ; 执行直接显存写入 (MMIO)。

    ; 硬件阻塞延时
    mov ah, 0x86        ; BIOS 中断 0x15 功能号：系统延时等待。
    mov cx, 0x0000      ; 延时参数高 16 位。
    mov dx, 0x86A0      ; 延时参数低 16 位。CX:DX = 34464 微秒。
    int 0x15            ; 触发中断，挂起处理器流水线。

    ; 数据变量状态机步进 
    inc byte [color]    ; 颜色属性循环递增。
    inc byte [char]     ; 字符 ASCII 码递增。
    cmp byte [char], '9' + 1 
    jne check_dir       ; 若字符未越界，跳转至方向逻辑。
    mov byte [char], '0'; 若超出 '9'，复位为 '0'。

check_dir:
    ; 边界碰撞检测与方向路由 
    mov al, [dir]       ; 读取当前方向状态。
    cmp al, 0
    je move_right
    cmp al, 1
    je move_down
    cmp al, 2
    je move_left
    cmp al, 3
    je move_up

move_right:
    inc word [x]        ; X 轴递增。
    cmp word [x], 79    ; 校验右边界 (79)。
    jl next_iter        ; 若未碰撞，跳转至下一帧。
    mov byte [dir], 1   ; 触发边界，状态机切换至 1 (下行)。
    jmp next_iter
move_down:
    inc word [y]
    cmp word [y], 24    ; 校验下边界 (24)。
    jl next_iter
    mov byte [dir], 2   ; 状态机切换至 2 (左行)。
    jmp next_iter
move_left:
    dec word [x]
    cmp word [x], 0     ; 校验左边界 (0)。
    jg next_iter
    mov byte [dir], 3   ; 状态机切换至 3 (上行)。
    jmp next_iter
move_up:
    dec word [y]
    cmp word [y], 0     ; 校验上边界 (0)。
    jg next_iter
    mov byte [dir], 0   ; 状态机切换至 0 (右行)。

next_iter:
    jmp main_loop       ; 返回主循环。
```



![image-20260330203428108](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330203428108.png)



### 4.2 向内绕圈

> spiral.asm

实现思路：在上一题的基础上，每次光标在与边界发生碰撞时，修改方向向量[dir]，同时对边界进行收缩，循环直到边界发生交叉，即左边界大于右边界或上边界大于下边界，之后跳出循环。

```assembly
spiral_loop:
    ; 遍历边界相交检测
    mov ax, [min_x]     ; 读取当前左边界阈值。
    cmp ax, [max_x]     ; 与右边界阈值比较。
    jg spiral_end       ; 若左边界大于右边界 (越界交叉)，跳出主循环。
    mov ax, [min_y]     ; 读取当前上边界阈值。
    cmp ax, [max_y]     ; 与下边界阈值比较。
    jg spiral_end       ; 若上边界大于下边界 (越界交叉)，跳出主循环。

    ; 显存线性物理偏移计算
    mov ax, [y]         ; 加载行坐标。
    mov cx, 80          ; 加载行跨度常量。
    mul cx              ; 无符号乘法：DX:AX = AX * CX。
    add ax, [x]         ; 累加列坐标。
    shl ax, 1           ; 逻辑左移 1 位，转换为字节偏移量。
    mov di, ax          ; 偏移量存入 DI。
    
    mov al, [char]      ; 加载当前字符。
    mov ah, [color]     ; 加载当前颜色属性。
    mov [es:di], ax     ; 执行直接显存写入 (MMIO)。

    ; 硬件中断延时
    mov ah, 0x86        ; BIOS 中断 0x15 功能号：系统延时。
    mov cx, 0x0000      ; 延时参数高 16 位。
    mov dx, 0x86A0      ; 延时参数低 16 位。CX:DX = 34464 微秒。
    int 0x15            ; 触发软中断挂起处理器流水线。

    ; 数据状态修改
    inc byte [color]    ; 颜色属性递增。
    inc byte [char]     ; 字符 ASCII 码递增。
    cmp byte [char], '9' + 1 
    jne check_dir       ; 检查是否超出字符 '9'。
    mov byte [char], '0'; 若越界则复位。

check_dir:
    ; 状态机分支路由
    mov al, [dir]       ; 读取当前方向标识。
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
    cmp ax, [max_x]     ; 校验当前坐标与右边界阈值。
    jl do_r             ; 若未触及边界，执行步进。
    mov byte [dir], 1   ; 触发边界，切换方向至 1 (下)。
    inc word [min_y]    ; 动态收缩上边界 (剥离已遍历行)。
    jmp move_d          ; 将控制流交由新方向处理。
do_r:
    inc word [x]        ; 坐标步进。
    jmp next_sp

move_d:
    mov ax, [y]
    cmp ax, [max_y]     ; 校验当前坐标与下边界阈值。
    jl do_d
    mov byte [dir], 2   ; 切换方向至 2 (左)。
    dec word [max_x]    ; 动态收缩右边界。
    jmp move_l
do_d:
    inc word [y]
    jmp next_sp

move_l:
    mov ax, [x]
    cmp ax, [min_x]     ; 校验当前坐标与左边界阈值。
    jg do_l
    mov byte [dir], 3   ; 切换方向至 3 (上)。
    dec word [max_y]    ; 动态收缩下边界。
    jmp move_u
do_l:
    dec word [x]
    jmp next_sp

move_u:
    mov ax, [y]
    cmp ax, [min_y]     ; 校验当前坐标与上边界阈值。
    jg do_u
    mov byte [dir], 0   ; 切换方向至 0 (右)。
    inc word [min_x]    ; 动态收缩左边界。
    jmp move_r
do_u:
    dec word [y]

next_sp:
    jmp spiral_loop     ; 返回循环入口。

spiral_end:
    jmp $               ; 无条件跳转至当前地址，形成处理器的无限空循环。
```



![image-20260330203623428](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330203623428.png)



![image-20260330204002873](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330204002873.png)



### 4.3 贪吃蛇 

> snake.asm

实现思路：利用键盘中断，根据键盘缓冲区中的输入，修改方向向量[dir]，之后按照一定的速度向该方向移动，每次进行新的渲染时，先检查是否超过边界并进行越界处理，之后将蛇打印在屏幕上。

```assembly
org 0x7c00              ; 伪指令：设定程序起始逻辑绝对偏移地址为 0x7C00。
[bits 16]               ; 伪指令：指示汇编器生成 16 位机器码。

;  视频模式初始化 
mov ax, 0x0003          ; 数据传送：将立即数 0x0003 装入 AX (AH=0x00, AL=0x03)。
int 0x10                ; 软中断：触发 BIOS 视频服务，功能号 0x00 设置视频模式为 80x25 彩色文本模式，硬件副作用为清空显存缓冲。

xor ax, ax              ; 异或指令：清零 AX 寄存器。
mov ds, ax              ; 数据传送：初始化数据段寄存器 (DS) 为 0x0000。
mov ax, 0xb800          ; 数据传送：将 VGA 文本模式显存段物理基址 0xB800 加载至 AX。
mov es, ax              ; 数据传送：初始化附加段寄存器 (ES) 为 0xB800。

; 初始坐标状态写入
mov word [x], 40        ; 内存直接写入：逻辑列坐标 (X) 初始值置 40。
mov word [y], 12        ; 内存直接写入：逻辑行坐标 (Y) 初始值置 12。
mov byte [dir], 0       ; 内存直接写入：方向状态向量初始值置 0。

snake_loop:
    ; 非阻塞式硬件输入轮询 
    mov ah, 01h         ; BIOS 中断 0x16 功能号：检测键盘缓冲区状态。
    int 16h             ; 触发键盘中断。此功能非阻塞，若缓冲区为空，置位 ZF (Zero Flag) = 1；若有数据，清除 ZF。
    jz update_pos       ; 条件跳转：若 ZF=1 (无输入)，直接跳转至位置更新例程。
    
    ; 键盘缓冲区读取与出队
    mov ah, 00h         ; BIOS 中断 0x16 功能号：读取并移除键盘缓冲区首字符。
    int 16h             ; 触发键盘中断。读取的 ASCII 码存入 AL，扫描码存入 AH。
    cmp al, 'w'         ; 比较指令：将输入字符与 ASCII 码 0x77 ('w') 对比。
    je set_w            ; 条件跳转：若相等 (ZF=1)，跳转至状态机赋值例程。
    cmp al, 'a'
    je set_a
    cmp al, 's'
    je set_s
    cmp al, 'd'
    je set_d
    jmp update_pos      ; 若输入非预设控制键，忽略并进入位置更新例程。

set_w: mov byte [dir], 1; 内存写入：方向状态机置 1 (纵向递减)。
       jmp update_pos   ; 无条件跳转。
set_a: mov byte [dir], 2; 内存写入：方向状态机置 2 (横向递减)。
       jmp update_pos
set_s: mov byte [dir], 3; 内存写入：方向状态机置 3 (纵向递增)。
       jmp update_pos
set_d: mov byte [dir], 4; 内存写入：方向状态机置 4 (横向递增)。

update_pos:
    ; 初始状态停滞检测
    cmp byte [dir], 0   ; 读取当前方向状态。
    je draw_snake       ; 若为 0 (初始态未发生跃迁)，跳过坐标运算直接执行渲染。

    ;  擦除原物理坐标数据
    mov ax, [y]         ; 加载当前逻辑行坐标。
    mov cx, 80          ; 加载行跨度常量。
    mul cx              ; 无符号乘法：计算当前行的基准偏移。
    add ax, [x]         ; 累加列坐标。
    shl ax, 1           ; 逻辑左移 1 位：等效于乘 2，转换为物理字节偏移量。
    mov di, ax          ; 将物理偏移量存入 DI。
    mov word [es:di], 0x0720 ; 内存写入：将 16 位常量 0x0720 (背景 0x0, 前景 0x7, 字符 0x20 为空格) 写入显存，覆盖历史数据。

    ;  坐标算术运算与环面拓扑边界处理 
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
    dec word [y]        ; Y 坐标自减。
    cmp word [y], 0     ; 校验上边界 (0)。
    jge draw_snake      ; 若未越界 (>= 0)，跳转至渲染。
    mov word [y], 24    ; 若发生越界，强制置位为对侧边界阈值 (24)。
    jmp draw_snake
go_a: 
    dec word [x]        ; X 坐标自减。
    cmp word [x], 0     ; 校验左边界 (0)。
    jge draw_snake
    mov word [x], 79    ; 强制置位为对侧边界阈值 (79)。
    jmp draw_snake
go_s: 
    inc word [y]        ; Y 坐标自增。
    cmp word [y], 24    ; 校验下边界 (24)。
    jle draw_snake      ; 若未越界 (<= 24)，跳转至渲染。
    mov word [y], 0     ; 强制置位为对侧边界阈值 (0)。
    jmp draw_snake
go_d: 
    inc word [x]        ; X 坐标自增。
    cmp word [x], 79    ; 校验右边界 (79)。
    jle draw_snake
    mov word [x], 0     ; 强制置位为对侧边界阈值 (0)。

draw_snake:
    ;  目标物理坐标数据写入
    mov ax, [y]         ; 重新计算运算后的逻辑坐标对应的物理偏移。
    mov cx, 80
    mul cx
    add ax, [x]
    shl ax, 1
    mov di, ax
    mov word [es:di], 0x0A4F ; 内存写入：将 16 位常量 0x0A4F (属性 0x0A, 字符 0x4F 为 'O') 写入目标显存。

    ;  硬件中断阻塞 (帧率控制) 
    mov ah, 0x86        ; BIOS 中断 0x15 功能号：系统延时等待。
    mov cx, 0x0001      ; 延时参数高 16 位。
    mov dx, 0x3880      ; 延时参数低 16 位。CX:DX = 0x00013880 (十进制 80000 微秒)。
    int 0x15            ; 触发中断，挂起流水线。

    jmp snake_loop      ; 无条件跳转：返回主循环入口。

; 数据段存储 
x: dw 40                ; 分配 16 位内存存储列坐标。
y: dw 12                ; 分配 16 位内存存储行坐标。
dir: db 0               ; 分配 8 位内存存储方向状态。

times 510 - ($ - $$) db 0 ; 扇区零填充伪指令。
dw 0xAA55                 ; MBR 标准有效性标识魔数。
```



![image-20260330204137367](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260330204137367.png)



## 课后思考题

1.请你谈谈对多层语言模型的理解，即为什么需要有机器语言、汇编语言和高级语言三层？

> 机器语言是硬件可以执行的语言，但是0101串是人类无法阅读的。高级语言方便开发，但是机器无法直接执行。通过汇编语句作为两者的转换枢纽。

2.请你描述下IA-32处理器的种类和用法，例如eax又可以分为哪几个寄存器来访问？esp的用途是什么？

> 8个通用寄存器：eax, ebx, ecx, edx, ebp, esp, esi,edi、6个段寄存器cs, ss, ds, es, fs, gs、标志寄存器eflags、指令地址寄存器eip。
>
> eax：ax(eax的低16位)、ah(ax的高8位)、al(ax的低8位)。
>
> esp：栈指针寄存器，专门用于指向堆栈的栈顶。

3.请查阅相关资料，说说eflags的各个位有什么含义？

> CF (Carry Flag, 位0): 进位标志。无符号数运算最高位产生进位或借位时置 1。
>
> PF (Parity Flag, 位2): 奇偶标志。运算结果最低字节中“1”的个数为偶数时置 1。
>
> AF (Auxiliary Flag, 位4): 辅助进位标志。运算结果的第 3 位向第 4 位产生进位/借位时置 1。
>
> ZF (Zero Flag, 位6): 零标志。运算结果为 0 时置 1（常用于 cmp 后的判断）。
>
> SF (Sign Flag, 位7): 符号标志。运算结果最高位为 1（负数）时置 1。
>
> TF (Trap Flag, 位8): 陷阱标志。置 1 时，CPU 进入单步调试模式。
>
> IF (Interrupt Enable Flag, 位9): 中断允许标志。置 1 时允许响应可屏蔽硬件中断。
>
> DF (Direction Flag, 位10): 方向标志。控制串操作指令（如 rep movsb）时地址指针是自增（DF=0）还是自减（DF=1）。
>
> OF (Overflow Flag, 位11): 溢出标志。有符号数运算结果超出可表示范围时置 1。

4.什么是线性地址？实模式的寻址模式是什么？地址空间大小如何？

> 线性地址： 逻辑地址（段地址:偏移地址）经过分段机制转换后得到的地址。
>
> 实模式的寻址模式：物理地址 = (段寄存器的值 << 4) + 偏移地址
>
> 地址空间大小：因为物理地址是 20 位的（16位左移4位 + 16位），所以寻址空间最大为 2^20 字节，即 1MB

5.nasm汇编中的内存寻址方式有哪些？语法是什么？请分别描述

> 直接寻址： 直接给出常数物理地址（偏移）。mov ax, [0x7c00]
>
> 寄存器间接寻址： 用寄存器中的值作为偏移地址。16位模式下只能用 bx, bp, si, di。mov ax, [bx]
>
> 基址变址寻址： 基址寄存器 + 变址寄存器。mov ax, [bx+si]
>
> 相对基址变址寻址： 寄存器组合再加上一个常数偏移量。mov ax, [bp+di+4]

6.在什么情况下会使用默认寄存器`cs`，`ds`，`ss`？如何避免CPU在计算线性地址时使用默认寄存器？

> **`cs` (代码段):** 取指令时默认使用（搭配 `ip/eip`）。
>
> **`ss` (堆栈段):** 隐式堆栈操作（`push/pop`）或使用了 `bp/ebp` 作为基址寻址时默认使用（如 `mov ax, [bp+2]`）。
>
> **`ds` (数据段):** 一般的数据访问默认使用（如 `mov ax, [bx]`, `mov ax, [0x1234]`）。
>
> 避免使用默认寄存器：显式指定段寄存器加冒号： `mov ax, [es:bx]` 强制使用 `es` 段。
