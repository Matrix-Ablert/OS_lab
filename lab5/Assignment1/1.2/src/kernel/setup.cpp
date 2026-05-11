#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;


extern "C" void setup_kernel()
{
    // 中断处理部件
    interruptManager.initialize();
    // 屏幕IO处理部件
    stdio.initialize();
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);
    //asm_enable_interrupt();

    // === 变量声明 ===
    int test_val = 42;
    int neg_val = -42;

    // === 基础格式测试 ===
    printf("========== Basic Format Test ==========\n");
    printf("%%c: %c\n", 'A');
    printf("%%s: %s\n", "Hello World!");
    printf("%%d: %d\n", -1234);
    printf("%%x: %x\n", 0x7abcdef0);

    // === 新增 %o 八进制测试 ===
    printf("========== %%o Octal Test ==========\n");
    printf("octal 255 = %o\n", 255);
    printf("octal 0   = %o\n", 0);
    printf("octal 8   = %o\n", 8);
    printf("octal 64  = %o\n", 64);

    // === 新增 %u 无符号十进制测试 ===
    printf("========== %%u Unsigned Test ==========\n");
    printf("unsigned -1 = %u\n", -1);
    printf("unsigned 0  = %u\n", 0);
    printf("unsigned 42 = %u\n", 42);

    // === 新增 %p 指针格式测试 ===
    printf("========== %%p Pointer Test ==========\n");
    printf("address of test_val: %p\n", &test_val);
    printf("address of neg_val:  %p\n", &neg_val);

    // === 新增 %0Nd 零填充测试 ===
    printf("========== %%0Nd Zero-Pad Test ==========\n");
    printf("%%08d (42)  = %08d\n", 42);
    printf("%%08d (-42) = %08d\n", -42);
    printf("%%04d (7)   = %04d\n", 7);
    printf("%%04x (255) = %04x\n", 255);
    printf("%%08o (255)= %08o\n", 255);
    printf("%%08u (42) = %08u\n", 42);

    printf("========== All Tests Done! ==========\n");

    //uint a = 1 / 0;
    asm_halt();
}
