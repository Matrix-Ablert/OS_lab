# ============================================================
# 自动检测平台：Ubuntu AMD64 / 银河麒麟 ARM64
# ============================================================
ARCH := $(shell uname -m)

ifeq ($(ARCH),aarch64)
    # ---- 银河麒麟 ARM64 (交叉编译) ----
    CROSS_PREFIX = $(HOME)/lab1/cross-tools/install/bin/i686-elf-
    ASM_COMPILER = nasm
    C_COMPILER   = $(CROSS_PREFIX)gcc
    CXX_COMPILER = $(CROSS_PREFIX)g++
    LINKER       = $(CROSS_PREFIX)ld
    OBJCOPY      = $(CROSS_PREFIX)objcopy
    CXX_COMPILER_FLAGS = -g -Wall -march=i386 -std=c++11 -m32 -nostdlib \
                         -fno-builtin -ffreestanding -fno-pic -fno-exceptions -fno-rtti
    LINKER_FLAGS = -N
    QEMU = qemu-system-x86_64 -drive file=$(RUNDIR)/hd.img,format=raw
    GDB  = gdb-multiarch
else
    # ---- Ubuntu AMD64 (本地编译) ----
    ASM_COMPILER = nasm
    C_COMPILER   = gcc
    CXX_COMPILER = g++
    LINKER       = ld
    OBJCOPY      = objcopy
    CXX_COMPILER_FLAGS = -g -Wall -march=i386 -std=c++11 -m32 -nostdlib \
                         -fno-builtin -ffreestanding -fno-pic
    LINKER_FLAGS = -N -melf_i386
    QEMU = qemu-system-i386 -hda $(RUNDIR)/hd.img
    GDB  = gdb
endif

SRCDIR = ../src
RUNDIR = ../run
BUILDDIR = build
INCLUDE_PATH = ../include

KERNEL_SOURCE = $(wildcard $(SRCDIR)/kernel/*.cpp)
CXX_SOURCE += $(KERNEL_SOURCE)
CXX_OBJ += $(KERNEL_SOURCE:$(SRCDIR)/kernel/%.cpp=%.o)

UTILS_SOURCE = $(wildcard $(SRCDIR)/utils/*.cpp)
CXX_SOURCE += $(UTILS_SOURCE)
CXX_OBJ += $(UTILS_SOURCE:$(SRCDIR)/utils/%.cpp=%.o)

ASM_SOURCE += $(wildcard $(SRCDIR)/utils/*.asm)
ASM_OBJ += $(ASM_SOURCE:$(SRCDIR)/utils/%.asm=%.o)

OBJ += $(CXX_OBJ)
OBJ += $(ASM_OBJ)

build : mbr.bin bootloader.bin kernel.bin kernel.o
	dd if=mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	dd if=kernel.bin of=$(RUNDIR)/hd.img bs=512 count=145 seek=6 conv=notrunc

mbr.bin : $(SRCDIR)/boot/mbr.asm
	$(ASM_COMPILER) -o mbr.bin -f bin -I$(INCLUDE_PATH)/ $(SRCDIR)/boot/mbr.asm

bootloader.bin : $(SRCDIR)/boot/bootloader.asm
	$(ASM_COMPILER) -o bootloader.bin -f bin -I$(INCLUDE_PATH)/ $(SRCDIR)/boot/bootloader.asm

entry.obj : $(SRCDIR)/boot/entry.asm
	$(ASM_COMPILER) -o entry.obj -g -f elf32 $(SRCDIR)/boot/entry.asm

kernel.bin : kernel.o
	$(OBJCOPY) -O binary kernel.o kernel.bin

kernel.o : entry.obj $(OBJ)
	$(LINKER) -o kernel.o $(LINKER_FLAGS) entry.obj $(OBJ) -e enter_kernel -Ttext 0x00020000

$(CXX_OBJ): $(CXX_SOURCE)
	$(CXX_COMPILER) $(CXX_COMPILER_FLAGS) -I$(INCLUDE_PATH) -c $(CXX_SOURCE)

asm_utils.o : $(SRCDIR)/utils/asm_utils.asm
	$(ASM_COMPILER) -o asm_utils.o -g -f elf32 $(SRCDIR)/utils/asm_utils.asm

clean:
	rm -f *.o* *.bin

run:
	$(QEMU) -serial null -parallel stdio -no-reboot

monitor:
	$(QEMU) -serial null -no-reboot -display none \
	    -monitor telnet:127.0.0.1:45475,server,nowait -daemonize
	@echo "QEMU Monitor: telnet 127.0.0.1 45475"

debug:
	$(QEMU) -S -s -parallel stdio -serial null &
	@sleep 1
ifeq ($(ARCH),aarch64)
	$(GDB) -q -tui -x $(RUNDIR)/gdbinit
else
	gnome-terminal -e "$(GDB) -q -tui -x $(RUNDIR)/gdbinit"
endif

test:
	echo $(CXX_OBJ)

# ============================================================
# 输出捕获 - 将 QEMU 运行结果保存到文件
# 用法:
#   make output                          -- 默认输出到 ../output.txt, 运行10秒
#   make output OUTPUT_DIR=/tmp QEMU_TIMEOUT=8  -- 自定义目录和超时
# ============================================================
OUTPUT_DIR = ..
OUTPUT_FILE = $(OUTPUT_DIR)/output.txt
QEMU_TIMEOUT = 10

output: build
	@echo ">>> Capturing QEMU output (timeout: $(QEMU_TIMEOUT)s)..."
	@timeout $(QEMU_TIMEOUT) script -q -c \
		"$(QEMU) -display curses -serial null -no-reboot 2>/dev/null" \
		$(OUTPUT_FILE) || true
	@echo ">>> Stripping ANSI escape codes..."
	@if command -v ansifilter >/dev/null 2>&1; then \
		echo ">>> (using ansifilter)"; \
		ansifilter --text $(OUTPUT_FILE) | \
		sed '/^Script started on/d; /^Script done on/d' \
		> $(OUTPUT_DIR)/output_clean.txt; \
	else \
		echo ">>> (ansifilter not found, using perl fallback)"; \
		perl strip_ansi.pl $(OUTPUT_FILE) > $(OUTPUT_DIR)/output_clean.txt; \
	fi
	@echo ">>> Done!"
	@echo ">>> Clean output: $(abspath $(OUTPUT_DIR))/output_clean.txt"
