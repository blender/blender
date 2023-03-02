#[cxx::bridge(namespace = "foo")]
mod bridge {
    extern "Rust" {
        fn print(slice: &[u64]);
    }
}

fn print(slice: &[u64]) {
    println!("Hello cxxbridge from foo/mod.rs! {:?}", slice);
}
