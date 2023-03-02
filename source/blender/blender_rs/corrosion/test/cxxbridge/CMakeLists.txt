if(CORROSION_TESTS_CXXBRIDGE)
    # There seems to be an upstream cxx issue, where the libcxx.rlib on the rust side uses
    # MD_DynamicRelease regardless of the actual profile. I don't know if it is possible to setup
    # cxx to use the correct version of the Runtimelibrary for MSVC, so for now we just don't support
    # this usecase. If someone is interested, please feel free to investigate and fix.
    if(Rust_CARGO_TARGET MATCHES "-msvc$")
        return()
    endif()
    corrosion_tests_add_test(cxxbridge_cpp2rust_1 "rust_bin"
        TEST_SRC_DIR cxxbridge_cpp2rust
        TEST_PASS_THROUGH_ARGS -DTEST_CXXBRIDGE_VARIANT1=ON
    )
    corrosion_tests_add_test(cxxbridge_cpp2rust_2 "rust_bin" TEST_SRC_DIR cxxbridge_cpp2rust)
    corrosion_tests_add_test(cxxbridge_rust2cpp "cxxbridge-exe")

    set_tests_properties("cxxbridge_cpp2rust_1_run_rust_bin"
        PROPERTIES PASS_REGULAR_EXPRESSION
            "main function"
    )
    set_tests_properties("cxxbridge_rust2cpp_run_cxxbridge-exe"
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Hello cxxbridge from lib.rs! \\[4, 5, 6\\]\r?\nHello cxxbridge from foo/mod.rs! \\[4, 5, 6\\]"
    )
endif()
