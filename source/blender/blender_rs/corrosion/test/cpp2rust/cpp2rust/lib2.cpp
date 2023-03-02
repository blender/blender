#include <iostream>
#include <stdint.h>

extern "C" void cpp_function2(char const *name) {
    std::cout << "Hello, " << name << "! I am Cpp library Number 2!\n";
}

extern "C" uint32_t get_42() {
    uint32_t v = 42;
    return v;
}
