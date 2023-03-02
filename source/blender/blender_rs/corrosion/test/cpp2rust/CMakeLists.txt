corrosion_tests_add_test(cpp2rust "rust-exe")

set_tests_properties("cpp2rust_run_rust-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, Rust! I am Cpp!\r?\nHello, Rust! I am Cpp library Number 2!\r?\nHello, Rust! I am Cpp library Number 3!"
        )
