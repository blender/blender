#![no_std]
use core::panic::PanicInfo;

#[no_mangle]
pub extern "C" fn rust_function() {}

#[panic_handler]
fn panic(_panic: &PanicInfo<'_>) -> ! {
    loop {}
}