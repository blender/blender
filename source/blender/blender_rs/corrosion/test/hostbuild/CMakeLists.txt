# FIXME: ONly test this when cross-compiling?
corrosion_tests_add_test(hostbuild "rust-host-program")

set_tests_properties("hostbuild_run_rust-host-program" PROPERTIES PASS_REGULAR_EXPRESSION
        "^ok\r?\nHello Rust Hostbuild, I am an external C function"
        )
# Run tests are disabled by default when cross-compiling, however we still want to test hostbuild!
# So we manually re-enable the test here.
if(CMAKE_CROSSCOMPILING)
    set_tests_properties("hostbuild_run_rust-host-program" PROPERTIES DISABLED FALSE)
endif()
