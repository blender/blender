cmake_minimum_required(VERSION 3.15)
project(ExternalCorrosionGenerator LANGUAGES C)

add_subdirectory(../../.. corrosion)

get_property(
    GENERATOR_EXE_LOCATION
    TARGET Corrosion::Generator PROPERTY IMPORTED_LOCATION
)

if (NOT GENERATOR_EXE_LOCATION STREQUAL CORROSION_GENERATOR_EXECUTABLE)
    message(
        FATAL_ERROR
        "\
Corrosion Generator not Imported Correctly:
    Corrosion::Generator IMPORTED_LOCATION: ${GENERATOR_EXE_LOCATION}
    CORROSION_GENERATOR_EXECUTABLE:         ${CORROSION_GENERATOR_EXECUTABLE}")
endif()
