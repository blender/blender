cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
corrosion_experimental_cbindgen(CRATE rust-lib HEADER_NAME "rust-lib.h")

add_executable(cpp-exe main.cpp)
set_property(TARGET cpp-exe PROPERTY CXX_STANDARD 11)
target_link_libraries(cpp-exe PUBLIC rust-lib)
