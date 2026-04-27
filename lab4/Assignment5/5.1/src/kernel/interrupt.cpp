#include "interrupt.h"
#include "os_type.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"

extern STDIO stdio;

int times = 0;
// 仅保留必要的全局变量用于 Assignment5: 记录时钟中断次数

InterruptManager::InterruptManager()
{
    initialize();
}

void InterruptManager::initialize()
{
    // 初始化中断计数变量
    times = 0;
    
    // 初始化IDT
    IDT = (uint32 *)IDT_START_ADDRESS;
    asm_lidt(IDT_START_ADDRESS, 256 * 8 - 1);

    for (uint i = 0; i < 256; ++i)
    {
        setInterruptDescriptor(i, (uint32)asm_unhandled_interrupt, 0);
    }

    // 初始化8259A芯片
    initialize8259A();
}

void InterruptManager::setInterruptDescriptor(uint32 index, uint32 address, byte DPL)
{
    IDT[index * 2] = (CODE_SELECTOR << 16) | (address & 0xffff);
    IDT[index * 2 + 1] = (address & 0xffff0000) | (0x1 << 15) | (DPL << 13) | (0xe << 8);
}

void InterruptManager::initialize8259A()
{
    // ICW 1
    asm_out_port(0x20, 0x11);
    asm_out_port(0xa0, 0x11);
    // ICW 2
    IRQ0_8259A_MASTER = 0x20;
    IRQ0_8259A_SLAVE = 0x28;
    asm_out_port(0x21, IRQ0_8259A_MASTER);
    asm_out_port(0xa1, IRQ0_8259A_SLAVE);
    // ICW 3
    asm_out_port(0x21, 4);
    asm_out_port(0xa1, 2);
    // ICW 4
    asm_out_port(0x21, 1);
    asm_out_port(0xa1, 1);

    // OCW 1 屏蔽主片所有中断，但主片的IRQ2需要开启
    asm_out_port(0x21, 0xfb);
    // OCW 1 屏蔽从片所有中断
    asm_out_port(0xa1, 0xff);
}

void InterruptManager::enableTimeInterrupt()
{
    uint8 value;
    // 读入主片OCW
    asm_in_port(0x21, &value);
    // 开启主片时钟中断，置0开启
    value = value & 0xfe;
    asm_out_port(0x21, value);
}

void InterruptManager::enableKeyboardInterrupt()
{
    uint8 value;
    asm_in_port(0x21, &value);
    // 解除主片 IRQ1 的屏蔽（清除 bit1）
    value = value & 0xfd;
    asm_out_port(0x21, value);
}

void InterruptManager::disableKeyboardInterrupt()
{
    uint8 value;
    asm_in_port(0x21, &value);
    // 屏蔽 IRQ1（置1）
    value = value | 0x02;
    asm_out_port(0x21, value);
}

void InterruptManager::setKeyboardInterrupt(void *handler)
{
    setInterruptDescriptor(IRQ0_8259A_MASTER + 1, (uint32)handler, 0);
}

void InterruptManager::disableTimeInterrupt()
{
    uint8 value;
    asm_in_port(0x21, &value);
    // 关闭时钟中断，置1关闭
    value = value | 0x01;
    asm_out_port(0x21, value);
}

void InterruptManager::setTimeInterrupt(void *handler)
{
    setInterruptDescriptor(IRQ0_8259A_MASTER, (uint32)handler, 0);
}

// 中断处理函数：只记录时钟中断次数并在屏幕下方显示
extern "C" void c_time_interrupt_handler()
{
    ++times;

    const int counter_row = 23;
    const int counter_col = 2;

    int t = times;
    char buf[12];
    int idx = 0;
    if (t == 0) {
        buf[idx++] = '0';
    } else {
        while (t > 0 && idx < (int)sizeof(buf) - 1) {
            buf[idx++] = '0' + (t % 10);
            t /= 10;
        }
    }
    for (int i = 0; i < idx; ++i) {
        char ch = buf[idx - 1 - i];
        stdio.print(counter_row, counter_col + i, ch, 0x07);
    }
    for (int i = idx; i < 8; ++i) {
        stdio.print(counter_row, counter_col + i, ' ', 0x07);
    }
}

// 将扫描码(SET 1)转换为 ASCII（支持 0-9, A-Z, 空格, 回车等）
static char scancode_to_ascii(uint8 scan)
{
    switch (scan)
    {
    // numbers
    case 0x02: return '1';
    case 0x03: return '2';
    case 0x04: return '3';
    case 0x05: return '4';
    case 0x06: return '5';
    case 0x07: return '6';
    case 0x08: return '7';
    case 0x09: return '8';
    case 0x0A: return '9';
    case 0x0B: return '0';

    // top row letters
    case 0x10: return 'Q';
    case 0x11: return 'W';
    case 0x12: return 'E';
    case 0x13: return 'R';
    case 0x14: return 'T';
    case 0x15: return 'Y';
    case 0x16: return 'U';
    case 0x17: return 'I';
    case 0x18: return 'O';
    case 0x19: return 'P';

    case 0x1E: return 'A';
    case 0x1F: return 'S';
    case 0x20: return 'D';
    case 0x21: return 'F';
    case 0x22: return 'G';
    case 0x23: return 'H';
    case 0x24: return 'J';
    case 0x25: return 'K';
    case 0x26: return 'L';

    case 0x2C: return 'Z';
    case 0x2D: return 'X';
    case 0x2E: return 'C';
    case 0x2F: return 'V';
    case 0x30: return 'B';
    case 0x31: return 'N';
    case 0x32: return 'M';

    case 0x39: return ' '; // space
    case 0x1C: return '\n'; // Enter
    default:
        return 0;
    }
}

extern "C" void c_keyboard_interrupt_handler()
{
    uint8 scan = 0;
    // 读取扫描码
    asm_in_port(0x60, &scan);

    // 只处理按下（make code），忽略释放代码
    if (scan & 0x80)
        return;

    char ch = scancode_to_ascii(scan);
    if (!ch)
        return;

    static bool typing_initialized = false;
    if (!typing_initialized)
    {
        // 将光标移动到一个安全位置以实现打字机效果
        stdio.moveCursor(14, 2);
        typing_initialized = true;
    }

    if (ch == '\n')
    {
        // 换行：移动到下一行开始
        uint pos = stdio.getCursor();
        uint row = pos / 80;
        uint newrow = row + 1;
        if (newrow >= 25)
            newrow = 24;
        stdio.moveCursor(newrow, 0);
    }
    else
    {
        // 打印字符并移动光标
        stdio.print((uint8)ch);
    }
}