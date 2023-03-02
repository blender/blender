#include "rust-lib.h"
#include <cassert>

int main(int argc, char **argv) {
    assert(is_magic_number(MAGIC_NUMBER));
    struct Point p1, p2;
    p1.x = 54;
    p2.x = 46;
    p1.y = 34;
    p2.y = 66;
    add_point(&p1, &p2);
    assert(p1.x == 100);
    assert(p2.x == 46);
    assert(p1.y == 100);
    assert(p2.y == 66);
    add_point(&p1, NULL);
    assert(p1.x == 100);
    assert(p1.y == 100);
}
