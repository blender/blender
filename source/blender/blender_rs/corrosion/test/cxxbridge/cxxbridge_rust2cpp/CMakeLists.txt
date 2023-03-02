cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
corrosion_add_cxxbridge(cxxbridge-cpp CRATE cxxbridge-crate MANIFEST_PATH rust FILES lib.rs foo/mod.rs)

add_executable(cxxbridge-exe main.cpp)
target_link_libraries(cxxbridge-exe PUBLIC cxxbridge-cpp)
