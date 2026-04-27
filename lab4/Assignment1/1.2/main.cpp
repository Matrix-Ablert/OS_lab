// main.cpp
#include <iostream>

// 声明外部的汇编函数，使用 C 链接方式防止名字修饰 (Name Mangling)
extern "C" int asm_add(int a, int b);
extern "C" int call_c_multiply_from_asm(int a, int b);

int main() {
    int a = 5;
    int b = 6;

    // 在 C++ 中调用汇编函数
    int add_result = asm_add(a, b);
    std::cout << "在 C++ 中调用汇编 asm_add(" << a << ", " << b << ") 的结果: " 
              << add_result << std::endl;

    // 在 C++ 中调用汇编函数，该汇编函数内部又调用了 C 函数
    int mul_result = call_c_multiply_from_asm(a, b);
    std::cout << "汇编调用 C 函数 c_multiply(" << a << ", " << b << ") 的结果: " 
              << mul_result << std::endl;

    return 0;
}