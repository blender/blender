# Adding Corrosion to your project

There are two fundamental installation methods that are supported by Corrosion - installation as a
CMake package or using it as a subdirectory in an existing CMake project. For CMake versions below
3.19 Corrosion strongly recommends installing the package, either via a package manager or manually
using CMake's installation facilities.
If you have CMake 3.19 or newer, we recommend to use either the [FetchContent](#fetchcontent) or the 
[Subdirectory](#subdirectory) method to integrate Corrosion.

## FetchContent
If you are using CMake >= 3.19 or installation is difficult or not feasible in
your environment, you can use the
[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module to include
Corrosion. This will download Corrosion and use it as if it were a subdirectory at configure time.

In your CMakeLists.txt:
```cmake
include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.3.2 # Optionally specify a commit hash, version tag or branch here
)
# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)
```

## Subdirectory
Corrosion can also be used directly as a subdirectory. This solution may work well for small
projects, but it's discouraged for large projects with many dependencies, especially those which may
themselves use Corrosion. Either copy the Corrosion library into your source tree, being sure to
preserve the `LICENSE` file, or add this repository as a git submodule:
```bash
git submodule add https://github.com/corrosion-rs/corrosion.git
```

From there, using Corrosion is easy. In your CMakeLists.txt:
```cmake
add_subdirectory(path/to/corrosion)
```

## Installation


Installation will pre-build all of Corrosion's native tooling (required only for CMake versions
below 3.19) and install it together with Corrosions CMake files into a standard location.
On CMake >= 3.19 installing Corrosion does not offer any speed advantages, unless the native
tooling option is explicitly enabled.

### Install from source

First, download and install Corrosion:
```bash
git clone https://github.com/corrosion-rs/corrosion.git
# Optionally, specify -DCMAKE_INSTALL_PREFIX=<target-install-path> to specify a 
# custom installation directory
cmake -Scorrosion -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# This next step may require sudo or admin privileges if you're installing to a system location,
# which is the default.
cmake --install build --config Release
```

You'll want to ensure that the install directory is available in your `PATH` or `CMAKE_PREFIX_PATH`
environment variable. This is likely to already be the case by default on a Unix system, but on
Windows it will install to `C:\Program Files (x86)\Corrosion` by default, which will not be in your
`PATH` or `CMAKE_PREFIX_PATH` by default.

Once Corrosion is installed, and you've ensured the package is available in your `PATH`, you
can use it from your own project like any other package from your CMakeLists.txt:
```cmake
find_package(Corrosion REQUIRED)
```

### Package Manager

#### Homebrew (unofficial)

Corrosion is available via Homebrew and can be installed via

```bash
brew install corrosion
```

Please note that this package is community maintained. Please also keep in mind that Corrosion follows
semantic versioning and minor version bumps (i.e. `0.3` -> `0.4`) may contain breaking changes, while 
Corrosion is still pre `1.0`.
Please read the release notes when upgrading Corrosion.
