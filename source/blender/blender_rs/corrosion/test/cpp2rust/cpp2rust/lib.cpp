#include <iostream>

extern "C" void cpp_function(char const *name) {
    std::cout << "Hello, " << name << "! I am Cpp!\n";
}
