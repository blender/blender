fn main() {
    assert_eq!(env!("REQUIRED_VARIABLE"), "EXPECTED_VALUE");
    assert_eq!(std::env::var("ANOTHER_VARIABLE").unwrap(), "ANOTHER_VALUE");
    let cargo_major = env!("COR_CARGO_VERSION_MAJOR")
        .parse::<u32>()
        .expect("Invalid Major version");
    let cargo_minor = env!("COR_CARGO_VERSION_MINOR")
        .parse::<u32>()
        .expect("Invalid Minor version");

    // The `[env]` section in `.cargo/config.toml` was added in version 1.56.
    if cargo_major > 1 || (cargo_major == 1 && cargo_minor >= 56) {
        // Check if cargo picks up the config.toml, which sets this additional env variable.
        let env_value = option_env!("COR_CONFIG_TOML_ENV_VAR")
            .expect("Test failure! Cargo >= 1.56.0 should set this environment variable");
        assert_eq!(env_value, "EnvVariableSetViaConfig.toml");
    }
}
