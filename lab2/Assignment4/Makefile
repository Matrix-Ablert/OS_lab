IMG = hd.img
QEMU = qemu-system-i386
QEMU_FLAGS = -drive format=raw,file=$(IMG) -vga std

.PHONY: all help clean circle spiral snake

all: help

help:
	@echo "请使用以下命令启动对应的程序："
	@echo "  make circle  - 运行 边缘回旋程序 (带Matrix烙印)"
	@echo "  make spiral  - 运行 向内绕圈程序 (带Matrix烙印)"
	@echo "  make snake   - 运行 穿墙贪吃蛇程序"
	@echo "  make clean   - 清理所有生成的 .bin 和 hd.img 文件"

$(IMG):
	qemu-img create $(IMG) 10m

%.bin: %.asm
	nasm -f bin $< -o $@

circle: $(IMG) circle.bin
	dd if=circle.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

spiral: $(IMG) spiral.bin
	dd if=spiral.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

snake: $(IMG) snake.bin
	dd if=snake.bin of=$(IMG) bs=512 count=1 seek=0 conv=notrunc
	$(QEMU) $(QEMU_FLAGS)

clean:
	rm -f *.bin $(IMG)
	@echo "清理完成！"
