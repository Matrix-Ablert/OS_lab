; asm_utils.asm
[bits 32]

global asm_add
global call_c_multiply_from_asm
extern c_multiply

; int asm_add(int a, int b)
asm_add:
    push ebp
    mov ebp, esp

    ; 获取参数
    ; [ebp + 4] 是返回地址
    ; [ebp + 8] 是参数 a
    ; [ebp + 12] 是参数 b
    mov eax, [ebp + 8]   ; 将 a 放入 eax
    add eax, [ebp + 12]  ; eax = eax + b

    pop ebp
    ret

; int call_c_multiply_from_asm(int a, int b)
call_c_multiply_from_asm:
    push ebp
    mov ebp, esp

    ; 准备调用 C 函数 int c_multiply(int a, int b)
    ; 参数从右向左压栈
    push dword [ebp + 12]  ; 压入参数 b
    push dword [ebp + 8]   ; 压入参数 a

    call c_multiply        ; 调用 C 函数，返回值会自动存放在 eax 中

    ; 恢复栈平衡 (清理刚才压入的 2 个 32 位参数，共 8 字节)
    add esp, 8

    pop ebp
    ret