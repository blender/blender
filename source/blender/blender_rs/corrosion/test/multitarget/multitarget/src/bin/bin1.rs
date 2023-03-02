use multitarget_lib::hello_world;

fn main() {
    hello_world();
    unsafe {
        multitarget_lib::cpp_function("bin1\0".as_ptr() as *const _);
    }
}
