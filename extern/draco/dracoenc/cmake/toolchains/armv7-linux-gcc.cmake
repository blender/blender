if (NOT DRACO_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_)
set(DRACO_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if ("${CROSS}" STREQUAL "")
  # Default the cross compiler prefix to something known to work.
  set(CROSS arm-linux-gnueabihf-)
endif ()

if (NOT ${CROSS} MATCHES hf-$)
  set(DRACO_EXTRA_TOOLCHAIN_FLAGS "-mfloat-abi=softfp")
endif ()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)
set(AS_EXECUTABLE ${CROSS}as)
set(CMAKE_C_COMPILER_ARG1
    "-march=armv7-a -mfpu=neon ${DRACO_EXTRA_TOOLCHAIN_FLAGS}")
set(CMAKE_CXX_COMPILER_ARG1
    "-march=armv7-a -mfpu=neon ${DRACO_EXTRA_TOOLCHAIN_FLAGS}")
set(CMAKE_SYSTEM_PROCESSOR "armv7")

endif ()  # DRACO_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_
