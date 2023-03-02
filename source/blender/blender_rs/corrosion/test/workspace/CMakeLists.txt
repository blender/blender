corrosion_tests_add_test(workspace "my_program")

set_tests_properties("workspace_run_my_program" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Ok\r?\n$"
        )

