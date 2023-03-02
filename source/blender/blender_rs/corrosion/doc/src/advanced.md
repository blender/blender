## What does corrosion do?

The specifics of what corrosion does should be regarded as an implementation detail and not relied on
when writing user code. However, a basic understanding of what corrosion does may be helpful when investigating
issues.

### FindRust

Corrosion maintains a CMake module `FindRust` which is executed when Corrosion is loaded, i.e. at the time
of `find_package(corrosion)`, `FetchContent_MakeAvailable(corrosion)` or `add_subdirectory(corrosion)` depending
on the method used to include Corrosion.

`FindRust` will search for installed rust toolchains, respecting the options prefixed with `Rust_` documented in
the [Usage](usage.md#corrosion-options) chapter.
It will select _one_ Rust toolchain to be used for the compilation of Rust code. Toolchains managed by `rustup`
will be resolved and corrosion will always select a specific toolchain, not a `rustup` proxy.


### Importing Rust crates

Corrosion's main function is `corrosion_import_crate`, which internally will call `cargo metadata` to provide
structured information based on the `Cargo.toml` manifest.
Corrosion will then iterate over all workspace and/or package members and find all rust crates that are either
a static (`staticlib`) or shared (`cdylib`) library or a `bin` target and create CMake targets matching the
crate name. Additionally, a build target is created for each imported target, containing the required build
command to create the imported artifact. This build command can be influenced by various arguments to 
`corrosion_import_crate` as well as corrosion specific target properties which are documented int the  
[Usage](usage.md) chapter.
Corrosion adds the necessary dependencies and also copies the target artifacts out of the cargo build tree
to standard CMake locations, even respecting `OUTPUT_DIRECTORY` target properties if set.

### Linking

Depending on the type of the crate the linker will either be invoked by CMake or by `rustc`.
Rust `staticlib`s are linked into C/C++ code via `target_link_libraries()` and the linker is
invoked by CMake.
For rust `cdylib`s and `bin`s, the linker is invoked via `rustc` and CMake just gets the final artifact.

#### CMake invokes the linker

When CMake invokes the linker, everything is as usual. CMake will call the linker with
the compiler as the linker driver and users can just use the regular CMake functions to
modify linking behaviour. The corrosion functions mentioned below have **no effect**.

#### Rustc invokes the linker 

Rust `cdylib`s and `bin`s are linked via `rustc`. Corrosion provides several helper functions
to influence the linker invocation for such targets. 

`corrosion_link_libraries()` is essentially the equivalent to `target_link_libraries()`, 
if the target is a rust `cdylib` or `bin`.
Under the hood this function passes `-l` and `-L` flags to the linker invocation and
ensures the linked libraries are built first.

`corrosion_set_linker()` can be used to specify a custom linker, in case the default one
chosen by corrosion is not what you want.
Corrosion currently instructs `rustc` to use the C/C++ compiler as the linker driver.
This is done because:
- For C++ code we must link with `libstdc++` or `libc++` (depending on the compiler), so we must
  either specify the library on the link line or use a `c++` compiler as the linker driver.
- `Rustc`s default linker selection currently is not so great. For a number of platforms
  `rustc` will fallback to `cc` as the linker driver. When cross-compiling, this leads
  to linking failures, since the linker driver is for the host architecture.
  Corrosion avoids this by specifying the C/C++ compiler as the linker driver.


In some cases, especially in older rust versions (pre 1.68), the linker flavor detection 
of `rustc` is also not correct, so when setting a custom linker you may want to pass the
[`-C linker-flavor`](https://doc.rust-lang.org/rustc/codegen-options/index.html#linker-flavor)
rustflag via `corrosion_add_target_local_rustflags()`.

## FFI bindings

For interaction between Rust and other languages there need to be some FFI bindings of some sort.
For simple cases manually defining the interfaces may be sufficient, but in many cases users
wish to use tools like [bindgen], [cbindgen], [cxx] or [autocxx] to automate the generating of
bindings.

In principle there are two different ways to generate the bindings:
- use a `build.rs` script to generate the bindings when cargo is invoked, using
  library versions of the tools to generate the bindings.
- use the cli versions of the tools and setup custom CMake targets/commands to
  generate the bindings. This approach should be preferred if the bindings are needed
  by the C/C++ side.

Corrosion currently provides 2 experimental functions to integrate cbindgen and cxx into
the build process. They are not 100% production ready yet, but should work well as a 
template on how to integrate generating bindings into your build process.

Todo: expand this documentation and link to other resources.

[bindgen]: https://rust-lang.github.io/rust-bindgen/
[cbindgen]: https://github.com/eqrion/cbindgen
[cxx]: https://cxx.rs/
[autocxx]: https://google.github.io/autocxx/index.html
  