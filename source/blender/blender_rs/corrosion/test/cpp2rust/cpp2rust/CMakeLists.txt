cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)

add_library(cpp-lib lib.cpp)
target_compile_features(cpp-lib PRIVATE cxx_std_14)
set_target_properties(
    cpp-lib
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

add_library(cpp-lib2 lib2.cpp)
target_compile_features(cpp-lib2 PRIVATE cxx_std_14)
set_target_properties(
        cpp-lib2
        PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME cpp-lib-renamed
)

add_library(cpp-lib3 "path with space/lib3.cpp" )
target_compile_features(cpp-lib3 PRIVATE cxx_std_14)
set_target_properties(
        cpp-lib3
        PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

corrosion_link_libraries(rust-exe cpp-lib cpp-lib2 cpp-lib3)
