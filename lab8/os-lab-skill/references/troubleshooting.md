# OS Lab Troubleshooting

## Makefile 缩进错误

**现象：** `make` 报 missing separator 或编译规则无法执行。

**原因：** Makefile 命令行必须以 Tab 开头，不能用空格替代。

**解决：** 检查报错行，把命令前导空格替换为真实 Tab；报告中说明是 Makefile 语法要求，不是编译器错误。

## QEMU `-serial stdio` 与 `-parallel stdio` 冲突

**现象：** 修改 QEMU 参数后 `make run` 退出，提示 stdio 被多个设备占用。

**原因：** QEMU 不允许串口和并口同时绑定标准输入输出。

**解决：** 保留 `-serial stdio`，将 `-parallel stdio` 改为 `-parallel none`，便于把内核输出重定向到终端。

## VGA 输出截断

**现象：** 多线程或同步实验输出很多，VGA 25 行屏幕滚动后早期数据丢失。

**原因：** VGA 文本模式可见行数有限，截图只能保留末尾输出。

**解决：** 增加串口输出，把 `print()` 同步写入 COM1，并用 QEMU `-serial stdio` 在终端保留完整日志。

## GDB 无源码行号

**现象：** `info line start_kernel` 返回 no line number information，但能解析函数地址。

**原因：** 内核或目标程序未包含调试信息，或者编译配置未启用 debug info。

**解决：** 检查编译配置是否启用 `CONFIG_DEBUG_INFO`；报告中区分"能断到符号"和"能映射源码行"。

## WSL 内核 headers 不匹配

**现象：** 在 WSL Ubuntu 上编译内核模块时找不到当前运行内核对应 headers。

**原因：** WSL 使用 Microsoft 定制内核，发行版仓库中的 headers 往往不匹配。

**解决：** 使用自己编译的 Linux 内核源码树作为模块构建目标，再放入 QEMU 的极简文件系统验证。

## QEMU Monitor 验证

**映射验证：** 使用 `info mem` 查看分页后的虚拟地址映射。

**物理内存验证：** 对虚拟地址写入固定值后，用 `xp 物理地址` 检查物理内存，例如写入 `0xDEADBEEF` 后确认物理页内容变化。

**报告写法：** 同时写出虚拟地址、转换得到的物理地址、monitor 命令和输出截图，形成完整证据链。
