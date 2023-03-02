pub const MAGIC_NUMBER: u64 = 0xABCD_EFAB;

#[derive(Debug)]
#[repr(C)]
pub struct Point {
    x: u64,
    y: u64,
}

impl Point {
    pub(crate) fn add(&mut self, rhs: &Point) {
        self.x = self.x.wrapping_add(rhs.x);
        self.y = self.y.wrapping_add(rhs.y);
    }
}

#[no_mangle]
pub extern "C" fn add_point(lhs: Option<&mut Point>, rhs: Option<&Point>) {
    if let (Some(p1), Some(p2)) = (lhs, rhs) {
        p1.add(p2);
        // Print something so we can let Ctest assert the output.
        println!("add_point Result: {:?}", p1);
    }
}

// simple test if the constant was exported by cbindgen correctly
#[no_mangle]
pub extern "C" fn is_magic_number(num: u64) -> bool {
    num == MAGIC_NUMBER
}
