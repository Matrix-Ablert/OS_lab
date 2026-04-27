#include "interrupt.h"
#include "os_type.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"

extern STDIO stdio;

int times = 0;

// 路径与显示相关全局变量
static bool rotation_initialized = false;
static int path_len = 0;
// 存储路径的行列
// 使用固定最大路径长度，避免运行时动态分配
#define MAX_PATH_LEN 256
static int path_x_arr[MAX_PATH_LEN];
static int path_y_arr[MAX_PATH_LEN];
static uint8 path_char_arr[MAX_PATH_LEN];
static uint8 path_color_arr[MAX_PATH_LEN];
static int *path_x = path_x_arr;
static int *path_y = path_y_arr;
static uint8 *path_char = path_char_arr;
static uint8 *path_color = path_color_arr;
// 移动偏移量（顺时针）
static int path_offset = 0;
// 每隔多少次中断更新一次显示（用来控制速度）
static int update_period = 2;
// 中断计数用于节拍
static int tick_count = 0;

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

// 中断处理函数
extern "C" void c_time_interrupt_handler()
{
    // 使用定时器节拍来控制刷新频率
    ++times;
    ++tick_count;

    // 第一次进入时做初始化：清屏并打印不可被擦除的中心文字，以及生成周边路径
    if (!rotation_initialized)
    {
        rotation_initialized = true;
        // 清屏（用空格 0x07）
        for (int r = 0; r < 25; ++r)
        {
            for (int c = 0; c < 80; ++c)
            {
                stdio.print(r, c, ' ', 0x07);
            }
        }

        // 在屏幕正中央（第12行，第32列偏移处）打印固定文本，不可被擦除
        const char *center = "22347055 Matrix";
        int center_row = 12; // 第12行
        int center_col = 32; // 第32列偏移处
        int i = 0;
        while (center[i])
        {
            stdio.print(center_row, center_col + i, center[i], 0x0A); // 黑底亮绿
            ++i;
        }

        // 构建屏幕边缘顺时针路径（只包含边界单字符位置）
        int rows = 25;
        int cols = 80;
        int top = 0;
        int bottom = rows - 1;
        int left = 0;
        int right = cols - 1;

        // 计算路径并填充数组： top row left->right, right col top+1->bottom, bottom row right-1->left, left col bottom-1->top+1
        int idx = 0;
        // top row
        for (int c = left; c <= right; ++c)
        {
            if (idx < MAX_PATH_LEN) { path_x[idx] = top; path_y[idx] = c; ++idx; }
        }
        // right col (exclude top corner already included)
        for (int r = top + 1; r <= bottom; ++r)
        {
            if (idx < MAX_PATH_LEN) { path_x[idx] = r; path_y[idx] = right; ++idx; }
        }
        // bottom row (exclude right corner already included)
        for (int c = right - 1; c >= left; --c)
        {
            if (idx < MAX_PATH_LEN) { path_x[idx] = bottom; path_y[idx] = c; ++idx; }
        }
        // left col (exclude bottom and top corners)
        for (int r = bottom - 1; r >= top + 1; --r)
        {
            if (idx < MAX_PATH_LEN) { path_x[idx] = r; path_y[idx] = left; ++idx; }
        }

        path_len = idx;

        // 初始化动画状态
        path_offset = 0;
        tick_count = 0;
        update_period = 8; // 调速：每8个时钟节拍移动一步（可调）
    }

    // 仅每隔 update_period 个中断更新一次显示，从而作为延时控制
    if (tick_count < update_period)
    {
        ++tick_count;
        return;
    }
    tick_count = 0;

    // 当前位置为 path_offset（不再擦除之前显示的字符，保留痕迹）
    int cur = path_offset % path_len;
    // 数字字符从 '0' 开始，按步进循环 0-9
    static int step_count = 0;
    char ch = '0' + (step_count % 10);

    // 背景分组与渐变设置：每 group_size 个字符为一组，共同一个背景色；palette 定义渐变
    static const uint8 palette_bg[] = {0, 1, 2, 3, 4, 5, 6, 7};
    static const int palette_len = 8;
    const int bg_group_size = 7;      // 每个背景组包含多少个相同背景的字符
    const int bg_shift_period = 8;    // 每多少步相位推进一次（用于让渐变有移动感，可调）

    int phase = (step_count / bg_shift_period) % palette_len;
    int group_idx = ((cur / bg_group_size) + phase) % palette_len;
    uint8 bg = palette_bg[group_idx];

    // 前景固定为亮白以保证可见性，背景使用计算得到的 bg
    uint8 fg = 0x0F; // 前景：亮白
    uint8 color = (uint8)((bg << 4) | (fg & 0x0F));

    int cx = path_x[cur];
    int cy = path_y[cur];
    stdio.print(cx, cy, ch, color);
    path_offset = (path_offset + 1) % path_len;
    ++step_count;
}