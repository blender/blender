use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn rust_function(name: *const c_char) {
    let name = unsafe { std::ffi::CStr::from_ptr(name).to_str().unwrap() };
    println!("Hello, {}! I am Rust!", name);

    #[cfg(not(feature = "one"))]
    compile_error!("Feature one is not enabled");
    #[cfg(not(feature = "two"))]
    compile_error!("Feature two is not enabled");
    #[cfg(not(feature = "three"))]
    compile_error!("Feature three is not enabled");
}
