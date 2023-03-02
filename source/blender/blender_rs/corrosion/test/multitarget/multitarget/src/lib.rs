use std::os::raw::c_char;

pub fn hello_world() {
    println!("Hello, world!");
}

extern "C" {
    pub fn cpp_function(name: *const c_char);
}
