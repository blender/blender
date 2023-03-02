cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)

add_executable(rustflags-cpp-exe main.cpp)
target_link_libraries(rustflags-cpp-exe PUBLIC rustflag-test-lib)

# Test --cfg=key="value" rustflag.
corrosion_add_target_rustflags(rustflag-test-lib --cfg=test_rustflag_cfg1="test_rustflag_cfg1_value")

# Test using a generator expression to produce a rustflag and passing multiple rustflags.
corrosion_add_target_rustflags(rustflag-test-lib
        --cfg=test_rustflag_cfg2="$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:>>,debug,release>"
        "--cfg=test_rustflag_cfg3"
)

corrosion_add_target_local_rustflags(rustflag-test-lib "--cfg=test_local_rustflag1")
corrosion_add_target_local_rustflags(rustflag-test-lib --cfg=test_local_rustflag2="value")
