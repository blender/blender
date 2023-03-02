
extern "C" {
    fn get_42() -> u32;
}
pub fn calls_ffi() {
    let res = unsafe { get_42()};
    assert_eq!(res, 42);
}
