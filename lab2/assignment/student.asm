; If you meet compile error, try 'sudo apt install gcc-multilib g++-multilib' first

%include "head.include"
; you code here

your_if:
    ; if a1 >= 40
    mov eax, [a1]
    cmp eax, 40
    jl .check_18            ; 如果小于 40，跳去检查 18
    
    ; if_flag = (a1 + 3) / 5
    add eax, 3
    mov ecx, 5
    cdq                     ; 除法 idiv 前，用 cdq 扩展符号
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
    
    ; 【关键修复】：解引用指针并按单字节写入
    mov edi, [while_flag]   ; 把 while_flag 指向的真实堆地址取到 edi
    mov [edi + ecx], al     ; 将 AL (1字节) 写入 while_flag[a2 * 2]

    ; ++a2
    inc dword [a2]
    jmp .while_start        

.while_end:

%include "end.include"

your_function:
    ; for i = 0; your_string[i] != '\0'; ++i
    mov esi, 0              ; i = 0
    
    ; 【关键修复】：获取字符串指针的真实地址
    mov edi, [your_string]  ; 把 your_string 指向的地址取到 edi

.func_loop:
    ; 取出 your_string[i] 
    movzx eax, byte [edi + esi] 
    cmp al, 0               ; 遇到 '\0' 结束
    je .func_end            

    pushad                  
    
    ; push (your_string[i] + 9) to stack
    add eax, 9              
    push eax                
    
    call print_a_char       
    
    add esp, 4              ; 恢复栈平衡
    
    popad                   

    inc esi                 ; ++i
    jmp .func_loop          

.func_end:
    ret                     ; 函数返回