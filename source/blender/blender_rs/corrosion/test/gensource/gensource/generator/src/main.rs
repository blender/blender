use std::io::Write;
fn main() -> Result<(), std::io::Error> {
    let out_name = std::env::args().skip(1).next().unwrap();
    let mut out_file = std::fs::File::create(out_name)?;
    Ok(write!(out_file, "const _: () = ();")?)
}
