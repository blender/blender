corrosion_tests_add_test(crate_type "cpp-exe")


set_tests_properties("crate_type_run_cpp-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "Hello from lib 1!\r?\nHello from lib 2!"
        )
