cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH Cargo.toml)

add_library(cpp-lib4 lib.cpp)
target_compile_features(cpp-lib4 PRIVATE cxx_std_14)
set_property(TARGET cpp-lib4 PROPERTY POSITION_INDEPENDENT_CODE ON)
corrosion_link_libraries(bin1 cpp-lib4)
corrosion_link_libraries(bin2 cpp-lib4)
corrosion_link_libraries(bin3 cpp-lib4)
