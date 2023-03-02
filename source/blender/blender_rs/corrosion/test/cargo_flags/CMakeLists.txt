corrosion_tests_add_test(cargo_flags "flags-exe")

set_tests_properties("cargo_flags_run_flags-exe" PROPERTIES PASS_REGULAR_EXPRESSION [[Hello, Cxx! I am Rust!]])
