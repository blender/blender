extern "C" void rust_function();

extern "C" void cpp_function() {
    // Fail on linking issues
    rust_function();
}