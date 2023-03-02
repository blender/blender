#[cfg(test_rustflag_cfg1 = "test_rustflag_cfg1_value")]
use std::os::raw::c_char;

#[no_mangle]
#[cfg(test_rustflag_cfg1 = "test_rustflag_cfg1_value")]
pub extern "C" fn rust_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust!", name);
}

#[no_mangle]
#[cfg(all(debug_assertions, test_rustflag_cfg2 = "debug"))]
pub extern "C" fn rust_second_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust in Debug mode again!", name);
}

#[no_mangle]
#[cfg(all(not(debug_assertions), test_rustflag_cfg2 = "release"))]
pub extern "C" fn rust_second_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust in Release mode again!", name);
}

#[no_mangle]
#[cfg(test_rustflag_cfg3)]
pub extern "C" fn rust_third_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I'm Rust again, third time the charm!", name);
    assert_eq!(some_dependency::some_function(), 42);
}

#[cfg(not(test_rustflag_cfg3))]
const _: [(); 1] = [(); 2];

#[cfg(not(test_local_rustflag1))]
const _: [(); 1] = [(); 2];

#[cfg(not(test_local_rustflag2 = "value"))]
const _: [(); 1] = [(); 2];
