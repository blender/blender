cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0 LANGUAGES CXX)
include(../../test_header.cmake)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)
corrosion_add_cxxbridge(cxxbridge-cpp CRATE rust_bin FILES lib.rs)
target_include_directories(cxxbridge-cpp PUBLIC "include")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux"
    OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"
        AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    )
)
    corrosion_add_target_local_rustflags(rust_bin "-Clink-arg=-fuse-ld=lld")
endif()

if(TEST_CXXBRIDGE_VARIANT1)
    # Variant 1: Merge the C++ User sources into the generated library target.
    target_sources(cxxbridge-cpp PRIVATE cpplib.cpp)
    corrosion_link_libraries(rust_bin cxxbridge-cpp)
else()
    # Variant 2: Create a separate C++ library and link both the User library and
    # the generated library into rust
    add_library(cpp_lib STATIC cpplib.cpp)
    target_link_libraries(cpp_lib PUBLIC cxxbridge-cpp)
    corrosion_link_libraries(rust_bin cpp_lib cxxbridge-cpp)
endif()