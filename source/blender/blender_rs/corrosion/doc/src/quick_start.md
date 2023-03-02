# Quick Start

You can add corrosion to your project via the `FetchContent` CMake module or one of the other methods
described in the [Setup chapter](setup_corrosion.md).
Afterwards you can import Rust targets defined in a `Cargo.toml` manifest file by using
`corrosion_import_crate`. This will add CMake targets with names matching the crate names defined
in the Cargo.toml manifest. These targets can then subsequently be used, e.g. to link the imported
target into a regular C/C++ target.

The example below shows how to add Corrosion to your project via `FetchContent`
and how to import a rust library and link it into a regular C/C++ CMake target.

```cmake
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.3.3 # Optionally specify a commit hash, version tag or branch here
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file
corrosion_import_crate(MANIFEST_PATH rust-lib/Cargo.toml)

add_executable(your_cool_cpp_bin main.cpp)

# In this example the the `Cargo.toml` file passed to `corrosion_import_crate` is assumed to have
# defined a static (`staticlib`) or shared (`cdylib`) rust library with the name "rust-lib".
# A target with the same name is now available in CMake and you can use it to link the rust library into
# your C/C++ CMake target(s).
target_link_libraries(your_cool_cpp_bin PUBLIC rust-lib)
```

Please see the [Usage chapter](usage.md) for a complete discussion of possible configuration options.
