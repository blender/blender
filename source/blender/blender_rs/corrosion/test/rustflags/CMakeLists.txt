corrosion_tests_add_test(rustflags "rustflags-cpp-exe")

set_tests_properties("rustflags_run_rustflags-cpp-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, Cpp! I'm Rust!\r?\nHello, Cpp again! I'm Rust in (Debug|Release) mode again!\r?\nHello, Cpp again! I'm Rust again, third time the charm!\r?\n$"
        )

corrosion_tests_add_test(cargo_config_rustflags "cargo_config_rustflags")
