# Makefile for Assignment 1.2

# 指定编译器和汇编器
CC = gcc
CXX = g++
ASM = nasm

# 指定编译选项 (-m32 生成 32 位代码)
CFLAGS = -m32 -Wall -c
CXXFLAGS = -m32 -Wall -c
ASMFLAGS = -f elf32
LDFLAGS = -m32

# 指定目标文件和最终可执行文件名称
OBJS = main.o c_func.o asm_utils.o
TARGET = main.out

# 默认构建目标
all: $(TARGET)

# 链接所有的 .o 文件生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJS)

# 编译 C++ 源文件
main.o: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o main.o

# 编译 C 源文件
c_func.o: c_func.c
	$(CC) $(CFLAGS) c_func.c -o c_func.o

# 编译汇编源文件
asm_utils.o: asm_utils.asm
	$(ASM) $(ASMFLAGS) asm_utils.asm -o asm_utils.o

# 伪目标：一键编译并运行
run: $(TARGET)
	./$(TARGET)

# 伪目标：清理生成的中间文件和可执行文件
clean:
	rm -f $(OBJS) $(TARGET)