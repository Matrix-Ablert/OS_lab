#include "asm_utils.h"
#include "os_type.h"

void print_string(const char *str,uint8 color) {
    // VGA 文本模式的显存起始地址是 0xb8000
    // 我们将其强转为 uint16 指针，因为每个字符占 2 个字节（16位）
    uint16 *video_memory = (uint16 *)0xb8002;
    
    // 使用 static 关键字记录当前光标的偏移量
    // 这样在多次调用 print_string 时，光标位置不会丢失，会接着往下写
    static uint32 cursor_offset = 0;

    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '\n') {
            // 处理换行符：
            // cursor_offset / 80 算出当前在第几行
            // 加 1 后乘 80，直接将光标移动到下一行的第 0 列
            cursor_offset = (cursor_offset / 80 + 1) * 80;
        } else {
            // 将颜色属性（高8位）和字符ASCII码（低8位）合并，写入显存
            video_memory[cursor_offset] = (color << 8) | str[i];
            cursor_offset++;
        }
        
        // 边界保护：如果超出了屏幕总大小 (80列 * 25行 = 2000个字符)
        if (cursor_offset >= 80 * 25) {
            cursor_offset = 0; // 简单重置回左上角（本次实验打印3行足够，不会触发）
        }
    }
}

extern "C" void setup_kernel()
{
    uint16*video_memory = (uint16 *)0xb8000;
    for(int i = 0;i < 80 * 25;i++) {
        video_memory[i] = (0x0f << 8) | ' ';
    }

    // asm_hello_world();
    print_string("SStudent_id:22347055\n",0x0f);
    print_string("Shenghang Wang\n",0x0a);
    print_string("Date: 2026-04-22\n",0x0c);
    
    while(1) {

    }
}