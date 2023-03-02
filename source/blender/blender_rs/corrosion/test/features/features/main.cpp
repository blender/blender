extern "C" void rust_function(char const *name);
extern "C" void rust_second_function(char const *name);
extern "C" void rust_third_function(char const *name);

int main(int argc, char **argv) {
    if (argc < 2) {
        rust_function("Cpp");
        rust_second_function("Cpp again");
        rust_third_function("Cpp again");
    } else {
        rust_function(argv[1]);
    }
}
