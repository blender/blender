corrosion_tests_add_test(envvar "program_requiring_rust_lib_with_envvar")

set_tests_properties("envvar_run_program_requiring_rust_lib_with_envvar" PROPERTIES PASS_REGULAR_EXPRESSION
        "Ok"
        )
