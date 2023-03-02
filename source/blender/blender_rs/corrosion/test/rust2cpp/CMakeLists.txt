corrosion_tests_add_test(rust2cpp "cpp-exe;cpp-exe-shared")

set_tests_properties("rust2cpp_run_cpp-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )

set_tests_properties("rust2cpp_run_cpp-exe-shared" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )
