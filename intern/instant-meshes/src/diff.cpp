#include "serializer.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Syntax: diff [application state 1.ply] [application state 2.ply]" << endl;
        return -1;
    }
    Serializer s1(argv[1]);
    cout << s1 << endl;
    Serializer s2(argv[2]);
    cout << s2 << endl;

    bool diff = s1.diff(s2);
    if (!diff)
        cout << "No differences found." << endl;

    return diff ? 1 : 0;
}
