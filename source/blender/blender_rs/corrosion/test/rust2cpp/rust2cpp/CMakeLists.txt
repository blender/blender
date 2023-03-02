cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)

add_executable(cpp-exe main.cpp)
target_link_libraries(cpp-exe PUBLIC rust-lib)

add_executable(cpp-exe-shared main.cpp)
target_link_libraries(cpp-exe-shared
        PUBLIC rust-lib-shared)
