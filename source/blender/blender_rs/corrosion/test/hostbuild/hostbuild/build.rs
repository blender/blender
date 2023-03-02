fn main() {
    let out_dir = std::env::var("OUT_DIR").unwrap();
    cc::Build::new()
        .file("src/lib.c")
        .compile("hello");

    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=hello");
    println!("cargo:rerun-if-changed=src/lib.c");
}