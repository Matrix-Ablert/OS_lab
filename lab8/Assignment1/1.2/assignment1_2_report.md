# Assignment 1.2 GDB 分析栈与段寄存器变化

## 一、实验目标

本实验使用 GDB 调试 `Assignment1/1.2`，也就是课程材料中的 `src/3` 第一个进程版本，在系统调用 `int 0x80` 前后观察 `CS`、`SS`、`ESP`、`EIP` 的变化，并分析 TSS 中 `esp0` 和 `ss0` 的作用。

本题不需要修改内核代码，重点是调试观察和结合代码解释特权级切换过程。

## 二、调试方法

进入实验目录并构建：

```bash
cd Assignment1/1.2/build
make build
```

启动 QEMU GDB stub：

```bash
qemu-system-i386 -hda ../run/hd.img -S -s -parallel stdio -serial null -no-reboot
```

另开一个终端连接 GDB：

```bash
cd Assignment1/1.2/build
gdb -q -x ../run/assignment1_2.gdb
```

本次构建中的关键符号地址如下：

```text
asm_start_process         0xc0022610
asm_system_call_handler   0xc0022667
asm_system_call           0xc00226a3
tss                       0xc0033740
```

因此设置 3 个关键断点：

```gdb
b *0xc00226bd   # asm_system_call 中的 int 0x80
b *0xc0022667   # asm_system_call_handler 入口
b *0xc00226a2   # asm_system_call_handler 末尾 iret
```

## 三、int 0x80 之前

程序断在 `asm_system_call` 的 `int 0x80` 指令处：

```text
cs      0x2b
ss      0x3b
esp     0x8048fb8
eip     0xc00226bd
eflags  0x212
eax     0x0
ebx     0x84
ecx     0x144
edx     0xc
esi     0x7c
edi     0x0
```

此时：

```text
CPL = CS & 0x3 = 0x2b & 0x3 = 3
```

说明 CPU 当前运行在用户态。`asm_system_call` 已经把系统调用号放入 `eax`，把参数放入 `ebx`、`ecx`、`edx`、`esi`、`edi`：

```text
eax = 0      # 系统调用号 syscall_0
ebx = 132
ecx = 324
edx = 12
esi = 124
edi = 0
```

此时观察到 TSS：

```text
tss.esp0 = 0xc00256a0
tss.ss0  = 0x10
```

这两个值会在接下来从用户态进入内核态时被 CPU 自动使用。

## 四、进入 asm_system_call_handler 之后

执行 `int 0x80` 后，程序断在 `asm_system_call_handler` 入口：

```text
cs      0x20
ss      0x10
esp     0xc002568c
eip     0xc0022667
eflags  0x12
eax     0x0
ebx     0x84
ecx     0x144
edx     0xc
esi     0x7c
edi     0x0
```

此时：

```text
CPL = CS & 0x3 = 0x20 & 0x3 = 0
```

说明 CPU 已经从用户态切换到内核态。新的 `SS=0x10` 来自 TSS 的 `ss0` 字段，新的内核栈顶来自 TSS 的 `esp0=0xc00256a0`。

进入中断时，因为发生了从 3 特权级到 0 特权级的切换，CPU 会自动完成以下动作：

1. 从当前 TSS 中读取 `ss0` 和 `esp0`。
2. 将 `SS` 设置为 `ss0=0x10`。
3. 将 `ESP` 切换到 `esp0=0xc00256a0` 附近的内核栈。
4. 在新的 0 特权级栈中压入用户态现场。
5. 跳转到 0x80 中断门对应的处理函数 `asm_system_call_handler`。

内核栈顶部内容：

```text
0xc002568c: 0xc00226bf  0x0000002b  0x00000212  0x08048fb8
0xc002569c: 0x0000003b
```

这些值依次表示：

```text
EIP     = 0xc00226bf    # int 0x80 后下一条指令
CS      = 0x2b          # 用户态代码段
EFLAGS  = 0x212
ESP     = 0x8048fb8     # 用户态栈指针
SS      = 0x3b          # 用户态栈段
```

也就是说，执行 `int 0x80` 时，CPU 自动把返回用户态所需的现场保存在当前进程的内核栈中。

## 五、执行 iret 之后

系统调用处理函数在 `asm_system_call_handler` 中执行完 `call dword[system_call_table + eax * 4]` 后，把返回值保存在 `eax` 中。本次 `syscall_0(132, 324, 12, 124, 0)` 的返回值是：

```text
eax = 0x250 = 592
```

断在 `iret` 前：

```text
cs      0x20
ss      0x10
esp     0xc002568c
eip     0xc00226a2
eax     0x250
```

此时 `esp` 又回到进入 `asm_system_call_handler` 时的位置，栈顶仍然是 CPU 自动压入的用户态现场：

```text
0xc002568c: 0xc00226bf  0x0000002b  0x00000212  0x08048fb8
0xc002569c: 0x0000003b
```

单步执行 `iret` 后：

```text
cs      0x2b
ss      0x3b
esp     0x8048fb8
eip     0xc00226bf
eflags  0x212
eax     0x250
```

此时：

```text
CPL = CS & 0x3 = 3
```

说明 CPU 已经回到用户态，并继续执行 `int 0x80` 后面的指令，也就是 `asm_system_call` 中的 `pop edi`。

## 六、TSS 中 esp0 和 ss0 的作用

TSS 的核心作用是在用户态进入内核态时，为 CPU 提供 0 特权级栈。

在本实验中，TSS 初始化位于 `src/kernel/program.cpp`：

```cpp
void ProgramManager::initializeTSS()
{
    memset((char *)address, 0, size);
    tss.ss0 = STACK_SELECTOR;
    ...
    asm_ltr(selector << 3);
}
```

这里设置：

```text
tss.ss0 = STACK_SELECTOR = 0x10
```

而 `tss.esp0` 在进程被调度时更新：

```cpp
void ProgramManager::activateProgramPage(PCB *program)
{
    if (program->pageDirectoryAddress)
    {
        tss.esp0 = (int)program + PAGE_SIZE;
        ...
    }
}
```

因此，每个用户进程被调度运行前，内核都会把 `tss.esp0` 设置为该进程 PCB 所在页的顶部。这样一来，当这个进程在用户态执行 `int 0x80` 时，CPU 可以自动切换到该进程自己的内核栈，而不会继续使用用户栈。

总结：

```text
ss0  决定进入内核态后使用哪个栈段。
esp0 决定进入内核态后内核栈从哪里开始使用。
```

如果没有 TSS 提供的 `ss0/esp0`，CPU 在从 CPL=3 切换到 CPL=0 时就不知道应该使用哪个内核栈，也无法安全保存用户态现场。

## 七、结合代码的执行流程

### 1. 用户态发起系统调用

`src/utils/asm_utils.asm` 中：

```asm
asm_system_call:
    mov eax, [ebp + 2 * 4]
    mov ebx, [ebp + 3 * 4]
    mov ecx, [ebp + 4 * 4]
    mov edx, [ebp + 5 * 4]
    mov esi, [ebp + 6 * 4]
    mov edi, [ebp + 7 * 4]
    int 0x80
```

系统调用号通过 `eax` 传递，最多 5 个参数通过 `ebx/ecx/edx/esi/edi` 传递。这样做是因为进入内核态后栈会从用户栈切换到内核栈，如果参数只放在用户栈中，内核态 C 函数就不能直接按当前 `esp/ebp` 取到参数。

### 2. CPU 完成特权级切换

`int 0x80` 触发中断门。由于当前 CPL=3，而中断处理函数位于内核代码段，所以 CPU 切换到 CPL=0，并从 TSS 中加载 `ss0/esp0`。

### 3. 内核态处理系统调用

`asm_system_call_handler` 中：

```asm
push ds
push es
push fs
push gs
pushad
...
push edi
push esi
push edx
push ecx
push ebx
call dword[system_call_table + eax * 4]
```

这里先保存寄存器现场，然后将系统调用参数压入内核栈，最后根据 `eax` 中的系统调用号到 `system_call_table` 中找到对应处理函数。

本实验中 `eax=0`，所以调用的是 `syscall_0`。

### 4. iret 返回用户态

`asm_system_call_handler` 末尾：

```asm
popad
pop gs
pop fs
pop es
pop ds
mov eax, [ASM_TEMP]
iret
```

`iret` 会从当前内核栈中弹出 CPU 之前自动保存的 `EIP/CS/EFLAGS/ESP/SS`。由于弹出的 `CS=0x2b`，其低 2 位为 3，所以 CPU 恢复到用户态继续执行。

## 八、回答实验问题

1. `int 0x80` 之前：

```text
CS=0x2b, SS=0x3b, ESP=0x8048fb8, EIP=0xc00226bd, CPL=3
```

2. 进入 `asm_system_call_handler` 之后：

```text
CS=0x20, SS=0x10, ESP=0xc002568c, EIP=0xc0022667, CPL=0
```

新的 `SS` 和 `ESP` 来自 TSS 中的 `ss0=0x10` 和 `esp0=0xc00256a0`。CPU 切换到内核栈后，又自动压入用户态现场，所以最终观察到的 `ESP` 是 `0xc002568c`。

3. 执行 `iret` 之后：

```text
CS=0x2b, SS=0x3b, ESP=0x8048fb8, EIP=0xc00226bf, CPL=3
```

这些值保存在当前进程的内核栈中，即：

```text
0xc002568c: EIP
0xc0025690: CS
0xc0025694: EFLAGS
0xc0025698: ESP
0xc002569c: SS
```

4. TSS 的 `esp0` 和 `ss0` 作用：

当 CPU 从用户态进入内核态时，`ss0` 提供 0 特权级栈段选择子，`esp0` 提供 0 特权级栈顶地址。它们共同决定系统调用进入内核后使用哪一个内核栈。当前实验中，`ss0=0x10`，`esp0=0xc00256a0`，对应当前进程 PCB 顶部的内核栈。
