use std::os::raw::c_char;

extern "C" {
    fn c_function(name: *const c_char);
}

fn main() {
    println!("ok");
    let name = b"Rust Hostbuild\0";
    unsafe {
        c_function(name.as_ptr() as _);
    }
}
