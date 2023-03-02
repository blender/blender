use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn rust_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust!", name);
}


#[cfg(debug_assertions)]
const _: () = assert!(false, "Debug assertions where not disabled via custom profile!");
