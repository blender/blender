if (NOT DRACO_CMAKE_TOOLCHAINS_ARMV7S_IOS_CMAKE_)
set(DRACO_CMAKE_TOOLCHAINS_ARMV7S_IOS_CMAKE_ 1)

if (XCODE)
  # TODO(tomfinegan): Handle arm builds in Xcode.
  message(FATAL_ERROR "This toolchain does not support Xcode.")
endif ()

set(CMAKE_SYSTEM_PROCESSOR "armv7s")
set(CMAKE_OSX_ARCHITECTURES "armv7s")

include("${CMAKE_CURRENT_LIST_DIR}/arm-ios-common.cmake")

endif ()  # DRACO_CMAKE_TOOLCHAINS_ARMV7S_IOS_CMAKE_
