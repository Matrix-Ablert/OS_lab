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
    // 启动时清屏，保证干净的显示环境
    stdio.clear();
    // 编程 8253/8254 可编程定时器：设置为期望的中断频率
    // 计时器输入时钟为 1193180 Hz，计数值 divisor = 1193180 / desired_freq
    {
        const uint32 desired_freq = 100; // 目标频率：100 Hz，可改为 50 等
        uint16 divisor = (uint16)(1193180 / desired_freq);
        // 控制字 0x36: Channel 0, LSB then MSB, Mode 3 (square wave), binary
        asm_out_port(0x43, 0x36);
        asm_out_port(0x40, (uint8)(divisor & 0xFF));       // 低8位
        asm_out_port(0x40, (uint8)((divisor >> 8) & 0xFF)); // 高8位
    }
    // 启用和设定时钟中断
    interruptManager.enableTimeInterrupt();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);
    // 启用键盘中断并设置处理函数（IRQ1 -> 向量 IRQ0_8259A_MASTER + 1）
    interruptManager.enableKeyboardInterrupt();
    interruptManager.setKeyboardInterrupt((void *)asm_keyboard_interrupt_handler);
    asm_enable_interrupt();
    asm_halt();
}

