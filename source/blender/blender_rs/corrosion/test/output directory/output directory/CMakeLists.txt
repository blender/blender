cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH proj1/Cargo.toml)

# Note: The output directories defined here must be manually kept in sync with the expected test location.
set_target_properties(rust_bin1
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_bin_targetprop"
        PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_bin_pdb_targetprop"

)
set_target_properties(rust_lib1 PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_archive_targetprop")
set_target_properties(rust_lib1
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_lib_targetprop"
        PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_lib_pdb_targetprop"
)

add_custom_command(TARGET cargo-build_rust_bin1 POST_BUILD
    COMMAND
    ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/another_dir"
    COMMAND
    ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PROPERTY:rust_bin1,LOCATION>" "${CMAKE_CURRENT_BINARY_DIR}/another_dir/moved_bin"
    )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_bin_var")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_binlib_pdb_var")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_archive_var")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/custom_lib_var")

corrosion_import_crate(MANIFEST_PATH proj2/Cargo.toml)

unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
unset(CMAKE_PDB_OUTPUT_DIRECTORY)
unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
unset(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
unset(CMAKE_PDB_OUTPUT_DIRECTORY)

add_executable(consumer consumer.cpp)
add_dependencies(consumer cargo-build_rust_lib1 cargo-build_rust_lib2)

target_link_libraries(consumer rust_lib1 rust_lib2)
