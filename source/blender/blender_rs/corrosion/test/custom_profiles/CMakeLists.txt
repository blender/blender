# The tests in this folder test specifying the cargo profile name via the --profile option.
# The built-in `test` and `bench` profiles are _not_ supported, because they output
# artifacts to a different location and add a hash to the artifact name.
if(Rust_VERSION VERSION_GREATER_EQUAL 1.57.0)

    corrosion_tests_add_test(custom_profiles_global "custom-profile-exe" TEST_SRC_DIR custom_profiles)
    corrosion_tests_add_test(custom_profiles_target_specific "custom-profile-exe"
        TEST_SRC_DIR custom_profiles
        PASS_THROUGH_ARGS -DCORROSION_TEST_USE_TARGET_SPECIFIC_OVERRIDE=ON
    )
    corrosion_tests_add_test(dev_profile "dev_bin" TEST_SRC_DIR basic_profiles CARGO_PROFILE dev)
    corrosion_tests_add_test(release_profile "release_bin" TEST_SRC_DIR basic_profiles CARGO_PROFILE release)

    set_tests_properties("custom_profiles_global_run_custom-profile-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )
    set_tests_properties("custom_profiles_target_specific_run_custom-profile-exe" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )
    set_tests_properties("dev_profile_run_dev_bin" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )
    set_tests_properties("release_profile_run_release_bin" PROPERTIES PASS_REGULAR_EXPRESSION
        "^Hello, Cpp! I'm Rust!\r?\n$"
        )

endif()
