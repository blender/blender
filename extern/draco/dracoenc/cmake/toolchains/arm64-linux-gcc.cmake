if (NOT DRACO_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_)
set(DRACO_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if ("${CROSS}" STREQUAL "")
  # Default the cross compiler prefix to something known to work.
  set(CROSS aarch64-linux-gnu-)
endif ()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)
set(AS_EXECUTABLE ${CROSS}as)
set(CMAKE_C_COMPILER_ARG1 "-march=armv8-a")
set(CMAKE_CXX_COMPILER_ARG1 "-march=armv8-a")
set(CMAKE_SYSTEM_PROCESSOR "arm64")

endif ()  # DRACO_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_
