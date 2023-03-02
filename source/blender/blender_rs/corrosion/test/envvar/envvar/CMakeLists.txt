cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH Cargo.toml)

corrosion_set_env_vars(rust-lib-requiring-envvar
        "ANOTHER_VARIABLE=ANOTHER_VALUE"
        "$<TARGET_PROPERTY:program_requiring_rust_lib_with_envvar,INDIRECT_VAR_TEST>"
        "COR_CARGO_VERSION_MAJOR=${Rust_CARGO_VERSION_MAJOR}"
        "COR_CARGO_VERSION_MINOR=${Rust_CARGO_VERSION_MINOR}"
)

add_executable(program_requiring_rust_lib_with_envvar main.cpp)

set_property(
    TARGET program_requiring_rust_lib_with_envvar
    APPEND
    PROPERTY INDIRECT_VAR_TEST
    "REQUIRED_VARIABLE=EXPECTED_VALUE"
)

target_link_libraries(program_requiring_rust_lib_with_envvar PUBLIC rust-lib-requiring-envvar)
