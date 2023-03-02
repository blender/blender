#[cfg(feature = "myfeature")]
use std::os::raw::c_char;

#[no_mangle]
#[cfg(feature = "myfeature")]
pub extern "C" fn rust_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust!", name);
}

#[no_mangle]
#[cfg(feature = "secondfeature")]
pub extern "C" fn rust_second_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust again!", name);
}

#[no_mangle]
#[cfg(feature = "thirdfeature")]
pub extern "C" fn rust_third_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust again, third time the charm!", name);
}

#[cfg(feature = "compile-breakage")]
const _: [(); 1] = [(); 2]; // Trigger a compile error to make sure that we succeeded in de-activating this feature
