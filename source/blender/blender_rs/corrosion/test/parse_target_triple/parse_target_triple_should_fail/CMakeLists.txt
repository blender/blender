# This test is supposed to ensure that the regex in _corrosion_parse_platform works as expected.
cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

# Check if parsing fails
_corrosion_parse_platform("Cargo.toml" "1.55" "x86_64-unknown-linux-gnu-toomuch")



