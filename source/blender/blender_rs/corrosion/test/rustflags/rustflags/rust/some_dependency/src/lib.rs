//! Test that the local rustflags are only passed to the main crate and not to dependencies.
#[cfg(test_local_rustflag1)]
const _: [(); 1] = [(); 2];

#[cfg(test_local_rustflag2 = "value")]
const _: [(); 1] = [(); 2];

pub fn some_function() -> u32 {
    42
}
