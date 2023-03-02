corrosion_tests_add_test(cbindgen_rust2cpp "cpp-exe" TEST_SRC_DIR rust2cpp)

set_tests_properties(cbindgen_rust2cpp_run_cpp-exe PROPERTIES PASS_REGULAR_EXPRESSION
        "^add_point Result: Point { x: 100, y: 100 }\r?\n$"
)
# Todo: We also should add a cpp2rust test with the following setup:
# - A rust lib that is used by a rust executable
# - cbindgen creates bindings for the rust-lib
# - c++ code uses the rust lib and is used in turn by the rust bin.
