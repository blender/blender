cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

corrosion_import_crate(MANIFEST_PATH Cargo.toml)

# Do not use `corrosion_add_target_rustflags()` here, since we want to test if the rustflag from `.cargo/config.toml`
# is picked up.

# Local rustflags should not interfere with `.cargo/config.toml`, so enable one.
corrosion_add_target_local_rustflags(cargo_config_rustflags "--cfg=local_rustflag")
