cmake_minimum_required(VERSION 3.15)
project(test_project VERSION 0.1.0)
include(../../test_header.cmake)

add_subdirectory(generator)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/src/foo.rs"
	DEPENDS $<TARGET_FILE:srcgen>
	COMMAND $<TARGET_FILE:srcgen> "${CMAKE_CURRENT_SOURCE_DIR}/src/foo.rs"
)

add_custom_target(after_generation DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/foo.rs")
add_custom_target(genexdebug COMMAND ${CMAKE_COMMAND} -E echo "Config DEBUG: $<TARGET_PROPERTY:srcgen,IMPORTED_LOCATION_DEBUG> Config Release: $<TARGET_PROPERTY:srcgen,IMPORTED_LOCATION_RELEASE> IMPORTED_LOCATION: $<TARGET_PROPERTY:srcgen,IMPORTED_LOCATION>")

corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)
add_dependencies(cargo-prebuild_generated after_generation)

# Simple test for corrosion_parse_package_version
corrosion_parse_package_version("${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" srcgen_version)
if (NOT "${srcgen_version}" VERSION_EQUAL "0.1.0")
	message(FATAL_ERROR "Test failed to parse expected version")
endif()
