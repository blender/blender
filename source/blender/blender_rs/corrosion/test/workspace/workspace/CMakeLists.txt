cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml CRATES member1 member2)

#NOTE: member3 also contains a binary called my_program, but that shouldn't be a problem since it is not imported
add_executable(my_program main.cpp)
target_link_libraries(my_program PUBLIC member1 member2)
