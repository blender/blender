
#[cfg(some_cargo_config_rustflag)]
fn print_line() {
    println!("Rustflag is enabled");
}

// test that local rustflags don't override global rustflags set via `.cargo/config`
#[cfg(local_rustflag)]
fn test_local_rustflag() {
    println!("local_rustflag was enabled");
}

fn main() {
    print_line();
    test_local_rustflag();
}
