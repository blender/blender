// Check that libraries located at a path containing a space can also be linked.

#include <iostream>

extern "C" void cpp_function3(char const *name) {
    std::cout << "Hello, " << name << "! I am Cpp library Number 3!\n";
}

