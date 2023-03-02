#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn rs_cxx_init();
    }
}

pub fn rs_cxx_init() {
    println!("rs_cxx_init executed.")
}

#[no_mangle]
pub extern "C" fn rs_c_init() {
    println!("rs_c_init executed.");
    rs_cxx_init();
}
