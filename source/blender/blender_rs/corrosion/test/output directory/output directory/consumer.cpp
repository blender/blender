#include <iostream>
#include <cstdlib>

extern "C" unsigned int ret_12();


int main(int argc, char *argv[])
{
    std::cout << "HI\n";
    unsigned int a = ret_12();
    if (a != 12) {
        return -1;
    }

    return 0;
}
