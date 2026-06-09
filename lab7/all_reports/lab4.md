# <center>Lab4 混合编程与中断</center>

**本次实验部分代码和注释参考自大模型。**

> 实验环境：Ubuntu 22.04 (x86_64 / WSL2)

---

## Assignment1 混合编程的基本思路

### 1.1 复现与分析

复现 Example 1，使用 Makefile 构建项目。

```shell
make
./main.out
```

![image-20260420112448211](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260420112448211.png)

可以看到程序成功输出了 `This is a function from C.` 和 `This is a function from C++.`，证明 C++ 调用汇编函数、汇编函数再分别调用 C 和 C++ 函数的混合编程链路全部畅通。

**核心机制分析：**

混合编程的本质是：无论是 C/C++ 代码还是汇编代码，最终都会被编译成可重定位文件（`.o`），再由链接器统一链接。因此，只要在链接阶段能找到函数的实现，调用关系就能成立。具体规则如下：

- **`global` 关键字**：在汇编代码中，用 `global function_from_asm` 将函数符号暴露给链接器，使其可以被外部的 C/C++ 代码引用。若不声明 `global`，该函数符号在 `.o` 文件中是局部符号，外部无法链接到。

- **`extern` 关键字（汇编中）**：当汇编代码需要调用一个在 C 文件中实现的函数时，需要在汇编代码顶部声明 `extern function_from_C`，告知汇编器该符号来自外部，链接阶段再由链接器完成地址填充。

- **`extern "C"` 关键字（C++ 中）**：C++ 支持函数重载，编译器会对函数名进行"名字修饰"（Name Mangling），例如 `function_from_CPP` 编译后的符号名会带上参数类型信息，不再是原来的名字。`extern "C"` 指示编译器以 C 语言规则处理该函数，禁止名字修饰，使得汇编代码可以通过原始函数名 `function_from_CPP` 找到它。

本例中 `asm_utils.asm` 的核心代码如下：

```assembly
[bits 32]
global function_from_asm
extern function_from_C
extern function_from_CPP

function_from_asm:
    call function_from_C
    call function_from_CPP
    ret
```

而在 `main.cpp` 中，声明汇编函数的方式为：

```cpp
extern "C" void function_from_asm();
```

这里同样使用了 `extern "C"`，保证链接器在寻找 `function_from_asm` 时使用无修饰的原始名称，与汇编中的 `global function_from_asm` 完全对应。

---

### 1.2 带参数的混合编程

在 Example 1 的基础上，实现带参数与返回值的混合编程。

![image-20260422191019200](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422191019200.png)

可以看到汇编函数 `asm_add` 和 C 函数 `c_multiply` 均正确返回了计算结果。

**实现思路：**

C/C++ 函数调用约定规定：参数从右向左依次压栈，返回值放在 `eax` 中，`[ebp+8]` 取第一个参数，`[ebp+12]` 取第二个参数。

**汇编函数 `asm_add(int a, int b)` 实现：**

```assembly
; int asm_add(int a, int b)
asm_add:
    push ebp
    mov ebp, esp

    ; [ebp + 4] 是返回地址
    ; [ebp + 8] 是参数 a
    ; [ebp + 12] 是参数 b
    mov eax, [ebp + 8]   ; 将 a 放入 eax
    add eax, [ebp + 12]  ; eax = eax + b，结果留在 eax 中作为返回值

    pop ebp
    ret
```

**汇编代码调用 C 函数 `c_multiply(int a, int b)` 的过程：**

```assembly
; int call_c_multiply_from_asm(int a, int b)
call_c_multiply_from_asm:
    push ebp
    mov ebp, esp

    ; 参数从右向左压栈
    push dword [ebp + 12]  ; 压入参数 b
    push dword [ebp + 8]   ; 压入参数 a

    call c_multiply        ; 调用 C 函数，返回值自动存放在 eax 中

    add esp, 8             ; 恢复栈平衡，清理 2 个 4 字节参数

    pop ebp
    ret
```

在 `main.cpp` 中通过 `extern "C"` 声明并调用这两个汇编函数，验证结果符合预期。

---

## Assignment2 使用 C/C++ 来编写内核

### 2.1 复现与修改

复现 Example 2，并将输出 "Hello World" 改为输出学号 `22347055`。

**复现结果：**

![image-20260422192628092](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422192628092.png)

**修改后结果：**

![image-20260422192909452](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422192909452.png)

修改了 `src/utils/asm_utils.asm` 中的 `asm_hello_world` 函数，将每个字符的 ASCII 值改为学号 `22347055` 的各位数字，颜色属性保持 `0x03`（黑底青字）：

```assembly
[bits 32]

global asm_hello_world

asm_hello_world:
    push eax
    xor eax, eax

    mov ah, 0x03   ; 青色
    mov al, '2'
    mov [gs:2 * 0], ax
    mov al, '2'
    mov [gs:2 * 1], ax
    mov al, '3'
    mov [gs:2 * 2], ax
    mov al, '4'
    mov [gs:2 * 3], ax
    mov al, '7'
    mov [gs:2 * 4], ax
    mov al, '0'
    mov [gs:2 * 5], ax
    mov al, '5'
    mov [gs:2 * 6], ax
    mov al, '5'
    mov [gs:2 * 7], ax

    pop eax
    ret
```

---

### 2.2 实现 `print_string` 函数

在 Example 2 的基础上，实现 `void print_string(const char *str, uint8 color)` 函数，直接操作 VGA 显存输出多行彩色文本。

![image-20260422200013389](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422200013389.png)

屏幕上成功以不同颜色输出了学号、姓名和日期三行文本。

**VGA 文本模式显存格式：**

VGA 文本模式下，显存起始地址为 `0xb8000`，屏幕共 80 列 × 25 行 = 2000 个字符。每个字符在显存中占 **2 字节**：低字节为字符的 ASCII 码，高字节为颜色属性。颜色属性的低 4 位为前景色，高 4 位为背景色。例如 `0x0A` 表示黑底亮绿字，`0x0C` 表示黑底红字，`0x0F` 表示黑底亮白字。

**`print_string` 核心实现：**

```cpp
void print_string(const char *str, uint8 color) {
    uint16 *video_memory = (uint16 *)0xb8002;
    static uint32 cursor_offset = 0;   // 用 static 保留多次调用间的光标位置

    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '\n') {
            // 换行：直接将光标跳到下一行首列
            cursor_offset = (cursor_offset / 80 + 1) * 80;
        } else {
            // 颜色属性(高8位) | ASCII码(低8位) 写入显存
            video_memory[cursor_offset] = (color << 8) | str[i];
            cursor_offset++;
        }
        if (cursor_offset >= 80 * 25) {
            cursor_offset = 0;
        }
    }
}
```

使用 `static` 关键字修饰 `cursor_offset`，使其在函数多次调用之间保持状态，从而自动记忆当前光标位置，实现连续输出多行文本。在 `setup_kernel` 中分别以白色、绿色和红色调用该函数：

```cpp
print_string("SStudent_id:22347055\n", 0x0f);  // 亮白
print_string("Shenghang Wang\n",        0x0a);  // 亮绿
print_string("Date: 2026-04-22\n",     0x0c);  // 红色
```

---

## Assignment3 中断的处理

### 3.1 实现页面错误中断处理

在 `src/8` 的基础上，仿照 Example 3 编写页面错误（Page Fault）中断处理函数，并将其注册到 IDT 的第 14 号中断描述符中。

![image-20260422205003769](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422205003769.png)

当 `setup.cpp` 中执行 `*(int*)0x100000 = 1;` 触发页面错误时，屏幕成功输出 "Page fault happened, halt..." 并停机。

**实现步骤：**

1. **编写中断处理程序**：在 `asm_utils.asm` 中新增 `asm_page_fault_interrupt`，输出提示信息后关中断并停机：

   ```assembly
   asm_page_fault_interrupt:
       cli
       ; 输出 "Page fault happened, halt..."
       ...
       jmp $
   ```

2. **声明处理函数**：在 `asm_utils.h` 头文件中声明：

   ```cpp
   extern "C" void asm_page_fault_interrupt();
   ```

3. **注册到 IDT**：在 `setup.cpp` 的 `setup_kernel()` 函数中，将第 14 号中断指向处理函数：

   ```cpp
   interruptManager.setInterruptDescriptor(14, (uint32)asm_page_fault_interrupt, 0);
   ```

4. **触发验证**：`setup.cpp` 中已有 `*(int*)0x100000 = 1;` 语句，由于该地址超出已映射的物理页范围，会产生页面错误，CPU 自动跳转到第 14 号中断处理程序。

---

### 3.2 读取 CR2 寄存器获取缺页地址

在 3.1 的基础上，进一步完善页面错误处理函数，读取 CR2 寄存器并将引发缺页的虚拟地址打印到屏幕上。

![image-20260422212252566](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260422212252566.png)

屏幕成功输出 `Page fault at 0x100000`，与 `setup.cpp` 中触发错误的地址完全一致。

**实现概述：**

- **`asm_read_cr2`（汇编）**：当页面错误发生时，CPU 自动将触发地址存入 CR2。编写一个汇编函数将其读出并通过 `eax` 返回：

  ```assembly
  ; uint32 asm_read_cr2()
  global asm_read_cr2
  asm_read_cr2:
      mov eax, cr2
      ret
  ```

- **`asm_page_fault_c_handler`（C++）**：在 C++ 中调用 `asm_read_cr2()` 获取地址，使用 `itos()` 转为十六进制字符串后打印：

  ```cpp
  extern "C" void asm_page_fault_c_handler() {
      unsigned int addr = asm_read_cr2();
      char buf[12];
      itos(buf, addr, 16);
      stdio.print("Page fault at 0x");
      stdio.print(buf);
      stdio.print("\n");
      asm_halt();
  }
  ```

- **IDT 注册**：在 `setup_kernel()` 中将第 14 号中断替换为新的 C++ 处理函数：

  ```cpp
  interruptManager.setInterruptDescriptor(14, (uint32)asm_page_fault_c_handler, 0);
  ```

> **注意事项**：`asm_read_cr2` 须在头文件中以 `extern "C"` 声明，以确保 C++ 和汇编之间的链接名称一致。`itos` 生成的是不带 `0x` 前缀的大写十六进制字符串，因此在 `print` 时手动前置了 `"0x"`。

---

### 3.3 思考题

**1. 引发段错误（页面错误）的几种方式：**

- **访问未映射的虚拟地址**：如 `*(int*)0x100000 = 1;`，该地址对应的页表项不存在，直接触发 Page Fault。
- **访问空指针**：`*(int*)0 = 1;`，地址 0 同样未被映射。
- **执行特权级违规访问**：在用户态尝试访问内核专属内存区域。
- **写入只读内存区域**：若对应页表项设置了写保护位，写操作会触发 Page Fault。
- **使用野指针**：指针指向任意随机未映射地址后进行读写。

**2. 为什么数组越界通常不触发段错误中断？**

在本实验的内核环境中，内存管理采用的是简单的恒等映射（Identity Mapping）或直接映射，内核地址空间内的地址基本全部可访问，并未建立严格的页面权限边界。数组越界后，访问的实际上是相邻的有效内存地址（仍在已映射的范围内），CPU 不会产生页面错误。真正的 Linux 用户程序之所以会因越界 Segfault，是因为有完善的 `mmap` 管理——数组所在区域之外的地址可能没有对应的物理页映射，越界后就会命中空洞。

**3. 哪些行为会真正引起页面错误中断？哪些理论上应该但实际上不会？**

| 行为 | 本内核是否触发 | 说明 |
|------|--------------|------|
| 访问 `0x100000` 以上的虚拟地址 | **会触发** | 超出页表已映射的前 256 页（0～1MB）范围 |
| 访问 NULL 指针（地址 0） | 视映射而定 | 若 0 地址已映射则不触发 |
| 数组越界（小幅度） | **不会触发** | 越界地址仍在已映射区域内 |
| 栈溢出（覆盖相邻区域） | **不会触发** | 内核未设置栈保护页 |
| 写入代码段 | **不会触发** | 内核未开启写保护 |

---

## Assignment4 时钟中断

### 4.1 字符回旋

**复现 Example 4：**

![image-20260423103014004](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260423103014004.png)

**使用 C/C++ 复刻的字符回旋程序：**

![image-20260427080809535](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260427080809535.png)

**实现思路：**

与 Lab2 汇编版的字符回旋不同，保护模式下时钟中断自带"延时"机制——每次中断发生后，我们只在边缘路径上打印**一个字符**，由中断频率自然控制动画速度，无需手动实现 delay 函数。

- **中断框架**：使用 8259A + IDT 的定时器中断。`setup.cpp` 通过 `interruptManager.setTimeInterrupt((void*)asm_time_interrupt_handler)` 绑定中断。汇编入口 `asm_time_interrupt_handler` 在发送 EOI 后调用 C++ 函数 `c_time_interrupt_handler` 完成绘制逻辑。

- **路径预计算**：首次进入中断处理函数时，一次性将屏幕四边的坐标按顺时针顺序写入静态数组 `path_x_arr[]` / `path_y_arr[]`，后续每次中断只需按 `path_offset` 索引取出坐标即可，避免在 ISR 中做复杂计算。

  ```cpp
  // 构建顺时针路径：上边 → 右边 → 下边 → 左边
  for (int c = 0; c <= right; ++c)       // 上边从左到右
      { path_x[idx] = 0;      path_y[idx] = c; ++idx; }
  for (int r = 1; r <= bottom; ++r)      // 右边从上到下
      { path_x[idx] = r;      path_y[idx] = right; ++idx; }
  for (int c = right-1; c >= 0; --c)    // 下边从右到左
      { path_x[idx] = bottom; path_y[idx] = c; ++idx; }
  for (int r = bottom-1; r >= 1; --r)   // 左边从下到上
      { path_x[idx] = r;      path_y[idx] = 0; ++idx; }
  ```

- **节拍控制**：用 `tick_count` 和 `update_period` 控制每隔多少个时钟中断才更新一次画面，避免动画过快：

  ```cpp
  if (tick_count < update_period) { ++tick_count; return; }
  tick_count = 0;
  ```

- **背景渐变**：定义颜色调色板 `palette_bg[]`，以分组相位的方式为每个字符分配背景色，产生彩虹渐变移动效果：

  ```cpp
  int phase     = (step_count / bg_shift_period) % palette_len;
  int group_idx = ((cur / bg_group_size) + phase) % palette_len;
  uint8 bg      = palette_bg[group_idx];
  uint8 color   = (bg << 4) | 0x0F;  // 背景渐变，前景固定亮白
  ```

- **freestanding 约束**：内核环境不支持动态内存分配，路径数组使用 `#define MAX_PATH_LEN 256` 的静态数组代替堆分配。

---

### 4.2 自定义时钟频率

在 4.1 的基础上，通过对 8253/8254 可编程定时器（PIT）编程，将默认时钟中断频率（约 18.2 Hz）修改为 **100 Hz**，并在屏幕上显示中断计数来验证频率。

![image-20260427083117205](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260427083117205.png)

屏幕第 23 行持续显示中断计数，观察可知每秒增量约为 100，与目标频率吻合。

**8253/8254 定时器编程方法：**

PIT（可编程间隔定时器）的 Channel 0 连接至 8259A 主片的 IRQ0，其输入时钟频率固定为 **1193180 Hz**。通过向其写入计数初值 $N$，可设定中断输出频率：

$$f = \frac{1193180}{N}$$

**编程步骤：**

1. 向端口 `0x43` 写入控制字 `0x36`，含义为：
   - `00`：选择 Channel 0
   - `11`：读写方式为先低字节后高字节（LSB then MSB）
   - `011`：工作模式 3（方波发生器）
   - `0`：二进制计数

2. 向端口 `0x40` 先写计数值低 8 位，再写高 8 位：

```cpp
const uint32 desired_freq = 100; // 目标：100 Hz
uint16 divisor = (uint16)(1193180 / desired_freq); // divisor = 11931
asm_out_port(0x43, 0x36);
asm_out_port(0x40, (uint8)(divisor & 0xFF));        // 低8位
asm_out_port(0x40, (uint8)((divisor >> 8) & 0xFF)); // 高8位
```

**示例计算：**

| 目标频率 | 计数值 $N$ | 实际频率 |
|---------|-----------|---------|
| 100 Hz  | 11931     | ≈ 100.008 Hz |
| 50 Hz   | 23864     | ≈ 49.999 Hz |

整数取整导致的微小误差可忽略不计，实测与理论值高度吻合。

**关键注意事项：** PIT 的设置必须在 `asm_enable_interrupt()`（即 `sti` 指令）执行之前完成，确保定时器在开中断前已正确配置；汇编中断入口负责发送 EOI，C++ 处理函数中不再重复发送。

---

## Assignment5 键盘中断（选做）

实现键盘中断处理程序，使内核能够响应键盘输入并实现"打字机"效果。

![image-20260427091951042](C:\Users\Wang\AppData\Roaming\Typora\typora-user-images\image-20260427091951042.png)

在 QEMU 中键入字母和数字，屏幕上对应位置成功显示对应字符，光标自动右移，Enter 键实现换行。

**实现思路：**

**初始化阶段：** 在 `setup_kernel()` 中按序完成 IDT 初始化、stdio 初始化、PIT 配置、时钟中断与键盘中断注册，最后开中断：

```cpp
interruptManager.setKeyboardInterrupt((void*)asm_keyboard_interrupt_handler);
interruptManager.enableKeyboardInterrupt();
```

键盘中断对应 8259A 主片的 **IRQ1**，中断向量号为 `IRQ0_8259A_MASTER + 1 = 0x21`。开启方式是清除 OCW1 中的 bit1：

```cpp
void InterruptManager::enableKeyboardInterrupt() {
    uint8 value;
    asm_in_port(0x21, &value);
    value = value & 0xfd;   // 清除 bit1，解除 IRQ1 屏蔽
    asm_out_port(0x21, value);
}
```

**中断处理流程：** 汇编入口 `asm_keyboard_interrupt_handler` 保护现场、发送 EOI，然后调用 C++ 函数 `c_keyboard_interrupt_handler()`。C++ 端从端口 `0x60` 读取扫描码：

```cpp
extern "C" void c_keyboard_interrupt_handler() {
    uint8 scan = 0;
    asm_in_port(0x60, &scan);
    if (scan & 0x80) return;   // 忽略释放码（bit7 = 1）
    char ch = scancode_to_ascii(scan);
    if (!ch) return;
    if (ch == '\n') {
        // 移动光标到下一行
        uint pos = stdio.getCursor();
        stdio.moveCursor(pos / 80 + 1, 0);
    } else {
        stdio.print((uint8)ch);
    }
}
```

**扫描码转 ASCII：** 使用 `switch-case` 查表，覆盖数字 0～9、26 个大写字母、空格和回车：

```cpp
static char scancode_to_ascii(uint8 scan) {
    switch (scan) {
    case 0x02: return '1'; case 0x03: return '2'; // ...数字
    case 0x10: return 'Q'; case 0x11: return 'W'; // ...字母
    case 0x39: return ' '; case 0x1C: return '\n';
    default:   return 0;
    }
}
```

**扫描码说明：** 键盘每次按键产生 Make Code（按下码），松开时产生 Break Code（释放码 = 按下码 + `0x80`）。通过判断 `scan & 0x80` 即可区分，本实现只响应按下事件。
