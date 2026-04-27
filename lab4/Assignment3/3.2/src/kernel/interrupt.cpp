#include "interrupt.h"
#include "os_type.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "stdlib.h"

int times = 0;

InterruptManager::InterruptManager()
{
    initialize();
}

void InterruptManager::initialize()
{
    // 初始化IDT
    IDT = (uint32 *)IDT_START_ADDRESS;
    asm_lidt(IDT_START_ADDRESS, 256 * 8 - 1);
    for (uint i = 0; i < 256; ++i)
    {
        setInterruptDescriptor(i, (uint32)asm_unhandled_interrupt, 0);
    }

}

void InterruptManager::setInterruptDescriptor(uint32 index, uint32 address, byte DPL)
{
    IDT[index * 2] = (CODE_SELECTOR << 16) | (address & 0xffff);
    IDT[index * 2 + 1] = (address & 0xffff0000) | (0x1 << 15) | (DPL << 13) | (0xe << 8);
}

// C-style page fault handler called from IDT entry. // 读取CR2寄存器获取导致页面错误的线性地址，并打印相关信息。
extern "C" void asm_page_fault_c_handler()
{
    // 读取CR2（导致页面错误的线性地址）并打印
    unsigned int addr = asm_read_cr2();
    char buf[9];
    itos(buf, addr, 16);
    stdio.print("Page fault at 0x");
    stdio.print(buf);
    stdio.print("\n");

    asm_halt();
}

