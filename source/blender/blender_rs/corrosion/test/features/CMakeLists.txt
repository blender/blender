corrosion_tests_add_test(features "features-cpp-exe")

set_tests_properties("features_run_features-cpp-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello, Cpp! I'm Rust!\r?\nHello, Cpp again! I'm Rust again!\r?\nHello, Cpp again! I'm Rust again, third time the charm!"
        )
