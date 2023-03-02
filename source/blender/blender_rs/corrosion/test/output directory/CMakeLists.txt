if(CMAKE_VERSION VERSION_LESS 3.19.0)
    return()
endif()

if(CMAKE_C_COMPILER)
    set(TEST_C_COMPILER "C_COMPILER" "${CMAKE_C_COMPILER}")
endif()
if(CMAKE_CXX_COMPILER)
    set(TEST_CXX_COMPILER "CXX_COMPILER" "${CMAKE_CXX_COMPILER}")
endif()
if(CMAKE_GENERATOR_PLATFORM)
    set(TEST_GENERATOR_PLATFORM "GENERATOR_PLATFORM" "${CMAKE_GENERATOR_PLATFORM}")
endif()

add_test(NAME "output_directory_build"
    COMMAND
        ${CMAKE_COMMAND}
        -P "${CMAKE_SOURCE_DIR}/test/ConfigureAndBuild.cmake"
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/output directory"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build"
        GENERATOR "${CMAKE_GENERATOR}"
        RUST_TOOLCHAIN "${Rust_TOOLCHAIN}"
        CARGO_TARGET "${Rust_CARGO_TARGET}"
        SYSTEM_NAME "${CMAKE_SYSTEM_NAME}"
        "${TEST_C_COMPILER}"
        "${TEST_CXX_COMPILER}"
        "${TEST_GENERATOR_PLATFORM}"

        COMMAND_EXPAND_LISTS
)
set_tests_properties("output_directory_build" PROPERTIES FIXTURES_SETUP "build_fixture_output_directory")
if(CORROSION_TESTS_INSTALL_CORROSION)
    set_tests_properties("output_directory_build" PROPERTIES FIXTURES_REQUIRED "fixture_corrosion_install")
endif()

foreach(output_approach targetprop var)
    if(output_approach STREQUAL "targetprop")
       set(rust_proj_suffix "1")
    elseif(output_approach STREQUAL "var")
        set(rust_proj_suffix "2")
    else()
        message(FATAL_ERROR "specify rust project suffix for new output approach ${output_approach}")
    endif()

    set(bin_name "rust_bin${rust_proj_suffix}${CMAKE_EXECUTABLE_SUFFIX}")

    add_test(NAME output_directory_bin_${output_approach}
            COMMAND
            "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/build/custom_bin_${output_approach}/${bin_name}"
    )
    set_tests_properties("output_directory_bin_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

    set(lib_name "rust_lib${rust_proj_suffix}")

    set(static_lib_name "${CMAKE_STATIC_LIBRARY_PREFIX}${lib_name}${CMAKE_STATIC_LIBRARY_SUFFIX}")

    add_test(NAME output_directory_staticlib_${output_approach}
            COMMAND
            "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/build/custom_archive_${output_approach}/${static_lib_name}"
    )
    set_tests_properties("output_directory_staticlib_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

    if(MINGW)
        # Windows-GNU defines "lib" as prefix for DLLs, but cargo creates foo.dll instead of libfoo.dll
        set(dynamic_lib_prefix "")
    else()
        set(dynamic_lib_prefix "${CMAKE_SHARED_LIBRARY_PREFIX}")
    endif()
    set(dynamic_lib_name "${dynamic_lib_prefix}${lib_name}${CMAKE_SHARED_LIBRARY_SUFFIX}")

    add_test(NAME output_directory_cdylib_${output_approach}
            COMMAND
            "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/build/custom_lib_${output_approach}/${dynamic_lib_name}"
    )
    set_tests_properties("output_directory_cdylib_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

    if(WIN32)
        set(implib_name ${CMAKE_IMPORT_LIBRARY_PREFIX}${lib_name}${CMAKE_IMPORT_LIBRARY_SUFFIX})

        add_test(NAME output_directory_implib_${output_approach}
            COMMAND
            "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
            # Implib is an ARCHIVE artifact, see:
            # https://cmake.org/cmake/help/v3.25/manual/cmake-buildsystem.7.html#archive-output-artifacts
            "${CMAKE_CURRENT_BINARY_DIR}/build/custom_archive_${output_approach}/${implib_name}"
            )
        set_tests_properties("output_directory_implib_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

        if(MSVC)
            if(output_approach STREQUAL "targetprop")
                set(expected_lib_pdb_path "custom_lib_pdb_targetprop")
                set(expected_bin_pdb_path "custom_bin_pdb_targetprop")
            elseif(output_approach STREQUAL "var")
                # When using a CMAKE_ variable instead of a target property, both targets
                # end up in the same directory.
                set(expected_lib_pdb_path "custom_binlib_pdb_var")
                set(expected_bin_pdb_path "custom_binlib_pdb_var")
            else()
                message(FATAL_ERROR "specify rust project suffix for new output approach ${output_approach}")
            endif()

            set(lib_pdb_name "${lib_name}.pdb")
            add_test(NAME output_directory_cdylib_pdb_${output_approach}
                COMMAND
                "${CMAKE_COMMAND}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/build/${expected_lib_pdb_path}/${lib_pdb_name}"
                )
            set_tests_properties("output_directory_cdylib_pdb_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

            set(bin_pdb_name "rust_bin${rust_proj_suffix}.pdb")
            add_test(NAME output_directory_bin_pdb_${output_approach}
                COMMAND
                "${CMAKE_COMMAND}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/build/${expected_bin_pdb_path}/${bin_pdb_name}"
                )
            set_tests_properties("output_directory_bin_pdb_${output_approach}" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")
        endif()
    endif()

endforeach()

add_test(NAME postbuild_custom_command
    COMMAND
    "${CMAKE_COMMAND}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/TestFileExists.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/build/another_dir/moved_bin"
    )
set_tests_properties("postbuild_custom_command" PROPERTIES FIXTURES_REQUIRED "build_fixture_output_directory")

add_test(NAME "output_directory_cleanup" COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/build")
set_tests_properties("output_directory_cleanup" PROPERTIES FIXTURES_CLEANUP "build_fixture_output_directory")
