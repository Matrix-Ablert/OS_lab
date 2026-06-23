#include "syscall.h"
#include "interrupt.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "os_modules.h"

int system_call_table[MAX_SYSTEM_CALL];

SystemService::SystemService() {
    initialize();
}

void SystemService::initialize()
{
    memset((char *)system_call_table, 0, sizeof(int) * MAX_SYSTEM_CALL);
    // 代码段选择子默认是DPL=0的平坦模式代码段选择子，DPL=3，否则用户态程序无法使用该中断描述符
    interruptManager.setInterruptDescriptor(0x80, (uint32)asm_system_call_handler, 3);
}

bool SystemService::setSystemCall(int index, int function)
{
    system_call_table[index] = function;
    return true;
}

// ========== 用户态包装函数 ==========

// 系统调用1号：max - 接收2个参数
int max(int a, int b) {
    return asm_system_call(1, a, b);
}

// 系统调用2号：factorial - 接收1个参数
int factorial(int n) {
    return asm_system_call(2, n);
}

// ========== 内核态处理函数 ==========

// 系统调用1号：返回两个整数的最大值
int syscall_max(int first, int second, int third, int forth, int fifth) {
    return (first > second) ? first : second;
}

// 系统调用2号：计算阶乘（迭代方式）
int syscall_factorial(int first, int second, int third, int forth, int fifth) {
    int n = first;  // 第一个参数作为 n
    if (n <= 1) return 1;
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}