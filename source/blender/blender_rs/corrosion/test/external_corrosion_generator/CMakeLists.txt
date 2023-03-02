if(CORROSION_TESTS_INSTALL_CORROSION)
    add_test(NAME "ExternalCorrosionGenerator"
            COMMAND
            ${CMAKE_COMMAND}
            -P "${CMAKE_SOURCE_DIR}/test/ConfigureAndBuild.cmake"
            SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ExternalCorrosionGenerator"
            BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build"
            GENERATOR "${CMAKE_GENERATOR}"
            RUST_TOOLCHAIN "${Rust_TOOLCHAIN}"
            EXTERNAL_CORROSION_GENERATOR "${test_install_path}/libexec/corrosion-generator"
            COMMAND_EXPAND_LISTS
            )
    set_tests_properties("ExternalCorrosionGenerator" PROPERTIES FIXTURES_REQUIRED "fixture_corrosion_install")
endif()