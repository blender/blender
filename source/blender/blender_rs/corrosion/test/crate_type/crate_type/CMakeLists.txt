cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

# Add --crate-type to ensure that only the specified type of library is built and no error is thrown
corrosion_import_crate(MANIFEST_PATH proj1/Cargo.toml CRATE_TYPES staticlib FLAGS --crate-type=staticlib)
corrosion_import_crate(MANIFEST_PATH proj2/Cargo.toml CRATE_TYPES cdylib FLAGS --crate-type=cdylib)

add_executable(cpp-exe main.cpp)
target_link_libraries(cpp-exe proj1)
target_link_libraries(cpp-exe proj2)
