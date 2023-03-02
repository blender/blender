# -----------------------------------------------------------------------------
# Build and link rust crate
cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(corrosion)

corrosion_import_crate(MANIFEST_PATH Cargo.toml)
corrosion_add_cxxbridge(blender_rs_bridge CRATE blender_rs MANIFEST_PATH Cargo.toml FILES lib.rs)

list(APPEND LIB blender_rs_bridge)
