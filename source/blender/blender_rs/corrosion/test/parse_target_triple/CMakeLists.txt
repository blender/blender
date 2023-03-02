corrosion_tests_add_test(parse_target_triple "")

set_tests_properties("parse_target_triple_build" PROPERTIES FAIL_REGULAR_EXPRESSION
        "CMake Warning"
        )
