extern "C" void rust_function(char const *name);


int main(int argc, char **argv) {
        rust_function("Cpp");
}
