cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml FEATURES thirdfeature ALL_FEATURES)

add_executable(features-cpp-exe main.cpp)
target_link_libraries(features-cpp-exe PUBLIC rust-feature-lib)

corrosion_set_features(rust-feature-lib
        ALL_FEATURES OFF
        NO_DEFAULT_FEATURES
        FEATURES
            $<TARGET_PROPERTY:features-cpp-exe,app_features>
)

set_property(
    TARGET features-cpp-exe
    APPEND
    PROPERTY app_features myfeature
)
set_property(
    TARGET features-cpp-exe
    APPEND
    PROPERTY app_features secondfeature
)

