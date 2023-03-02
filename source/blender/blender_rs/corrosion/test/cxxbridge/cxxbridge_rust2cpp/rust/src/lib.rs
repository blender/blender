mod foo;

#[cxx::bridge(namespace = "lib")]
mod bridge {
    extern "Rust" {
        fn print(slice: &[u64]);
    }
}

fn print(slice: &[u64]) {
    println!("Hello cxxbridge from lib.rs! {:?}", slice);
}
