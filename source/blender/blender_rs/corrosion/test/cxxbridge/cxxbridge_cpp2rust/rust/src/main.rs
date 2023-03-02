use cxxbridge_lib::ffi::{RsImage,Rgba,read_image};

fn main() {
    println!("main function");
    let expected = RsImage { width: 1, height: 1, raster: Rgba {
        r: 1.0,
        g: 2.0,
        b: 3.0,
        a: 4.0,
    }};
    let actual = read_image("dummy path");
    assert_eq!(actual, expected)
}