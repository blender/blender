extern "C" void rust_function(char const *name);

int main(int argc, char **argv) {
    if (argc < 2) {
        rust_function("Cpp");
    } else {
        rust_function(argv[1]);
    }
}
