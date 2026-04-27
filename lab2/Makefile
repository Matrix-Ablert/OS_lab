# 定义变量，方便以后修改
IMG = hd.img
QEMU = qemu-system-i386
QEMU_FLAGS = -drive format=raw,file=$(IMG) -vga std

# 默认目标：如果只敲 make，就打印帮助信息
.PHONY: all help clean circle spiral snake

all: help

help:
	@echo "请使用以下命令启动对应的程序："
	@echo "  make circle  - 运行 边缘回旋程序 (带Matrix烙印)"
	@echo "  make spiral  - 运行 向内绕圈程序 (带Matrix烙印)"
	@echo "  make snake   - 运行 穿墙贪吃蛇程序"
	@echo "  make clean   - 清理所有生成的 .bin 和 hd.img 文件"

# 检查并创建 10MB 虚拟硬盘 (如果已存在则不会重复创建)
$(IMG):
	qemu-img create $(IMG) 10m

# 通用编译规则：如何把 .asm 变成 .bin
%.bin: %.asm
	nasm -f bin $< -o $@

# --- 目标：边缘回旋 ---
circle: $(IMG) circle.bin
	dd if=circle.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

# --- 目标：向内绕圈 ---
spiral: $(IMG) spiral.bin
	dd if=spiral.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

# --- 目标：贪吃蛇 ---
snake: $(IMG) snake.bin
	dd if=snake.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

# --- 清理编译产物 ---
clean:
	rm -f *.bin $(IMG)
	@echo "清理完成！"