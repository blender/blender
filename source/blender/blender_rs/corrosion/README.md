# Corrosion
[![Build Status](https://github.com/corrosion-rs/corrosion/actions/workflows/test.yaml/badge.svg)](https://github.com/corrosion-rs/corrosion/actions?query=branch%3Amaster)
[![Documentation](https://img.shields.io/badge/docs-latest-blue.svg)](https://corrosion-rs.github.io/corrosion/)
![License](https://img.shields.io/badge/license-MIT-blue)

Corrosion, formerly known as cmake-cargo, is a tool for integrating Rust into an existing CMake
project. Corrosion can automatically import executables, static libraries, and dynamic libraries
from a workspace or package manifest (`Cargo.toml` file).

## Features
- Automatic Import of Executable, Static, and Shared Libraries from Rust Crate
- Easy Installation of Rust Executables
- Trivially Link Rust Executables to C/C++ Libraries in Tree
- Multi-Config Generator Support
- Simple Cross-Compilation

## Sample Usage with FetchContent

Using the CMake `FetchContent` module allows you to easily integrate corrosion into your build.
Other methods including installing corrosion or adding it as a subdirectory are covered in the
[setup chapter](https://corrosion-rs.github.io/corrosion/setup_corrosion.html) of the 
corrosion [documentation](https://corrosion-rs.github.io/corrosion/).

```cmake
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.3 # Optionally specify a commit hash, version tag or branch here
)
FetchContent_MakeAvailable(Corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file
corrosion_import_crate(MANIFEST_PATH rust-lib/Cargo.toml)

add_executable(your_cpp_bin main.cpp)
target_link_libraries(your_cpp_bin PUBLIC rust-lib)
```

