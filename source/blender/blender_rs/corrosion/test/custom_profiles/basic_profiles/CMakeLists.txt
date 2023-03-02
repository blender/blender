cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

if(NOT DEFINED CARGO_PROFILE)
    message(FATAL_ERROR "Test internal error. The test should be called with the CARGO_PROFILE parameter.")
endif()

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml PROFILE ${CARGO_PROFILE})

add_executable(${CARGO_PROFILE}_bin main.cpp)
target_link_libraries(${CARGO_PROFILE}_bin PUBLIC cargo-profiles-lib)
