# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# CMake toolchain file wrapper around the Android NDK toolchain to set values
# such as targetted ABI, minimum SDK version, etc... prior to including it.

# ----------------------------------------------------------------------------
# Android NDK discovery

# May be set explicitly by setting ANDROID_NDK_ROOT. Otherwise we try to infer the most
# recent NDK version installed on the host from default paths.
if(NOT DEFINED ANDROID_NDK_ROOT)
  if(DEFINED ENV{ANDROID_HOME})
    # ANDROID_HOME is commonly set to point to the base SDK root, used by Android Studio and others.
    set(_android_sdk_dir "$ENV{ANDROID_HOME}")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(_android_sdk_dir "$ENV{HOME}/Library/Android/sdk")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(_android_sdk_dir "$ENV{LOCALAPPDATA}/Android/Sdk")
  else()
    set(_android_sdk_dir "$ENV{HOME}/Android/Sdk")
  endif()

  file(GLOB _ndk_dirs "${_android_sdk_dir}/ndk/*")
  list(SORT _ndk_dirs COMPARE NATURAL)
  list(POP_BACK _ndk_dirs ANDROID_NDK_ROOT)

  if(NOT EXISTS ${ANDROID_NDK_ROOT})
    message(FATAL_ERROR
      "Failed to infer Android NDK root location, tried: ${_android_sdk_dir}/ndk/*. "
      "Ensure you have one installed, or manually set ANDROID_NDK_ROOT to its location. "
      "To obtain the Android NDK, Android Studio or the Android sdkmanager may be used."
    )
  endif()

  unset(_android_sdk_dir)
  unset(_ndk_dirs)
endif()

set(NDK_TOOLCHAIN_FILE "${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")

if(NOT EXISTS ${NDK_TOOLCHAIN_FILE})
  message(FATAL_ERROR
    "Could not find Android NDK toolchain file at ${NDK_TOOLCHAIN_FILE}."
  )
else()
  message(STATUS "Using Android NDK: ${ANDROID_NDK_ROOT}")
endif()

# ----------------------------------------------------------------------------
# Android ABI

# Supported ABIs are arm64-v8a (default) and x86_64.
# 32-bit ABIs (armeabi-v7a, x86) are not supported.
if(NOT ANDROID_ABI)
  set(ANDROID_ABI arm64-v8a)
endif()

if(NOT (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "x86_64"))
  message(FATAL_ERROR
    "Unsupported Android ABI \"${ANDROID_ABI}\", expected one of: arm64-v8a, x86_64."
  )
endif()

message(STATUS "Using Android ABI: ${ANDROID_ABI}")

# ----------------------------------------------------------------------------
# Android platform / minimum SDK version.

# Set to API level 29, minimum set to Android 10. See https://apilevels.com.

# - Used to be 23 (min Android 6.0), bumped to 28 to support C11 aligned_alloc required by OpenJPH.
#   It might be possible to lower it back down by patching OpenJPH.
# - Used to be 28 (min Android 9.0), bumped to 29 to support timespec_get for BLI's uuid.cc.
#   It should be possible to lower it back down by adding an ifdef switch in uuid.cc

set(ANDROID_PLATFORM android-29)

# ----------------------------------------------------------------------------
# Android STL type (static/shared)

# Using the shared due to the use of multiple shared libraries.
# See: https://developer.android.com/ndk/guides/cpp-support#shared_runtimes

set(ANDROID_STL c++_shared)

# ----------------------------------------------------------------------------
# Use Legacy Android NDK CMake toolchain file

# Despite its name, the legacy Android toolchain is the main supported NDK CMake toolchain,
# with the "new" toolchain file including behavior regressions.
# See: https://developer.android.com/ndk/guides/cmake#the_new_toolchain_file

set(ANDROID_USE_LEGACY_TOOLCHAIN ON)

# ----------------------------------------------------------------------------
# Main Android NDK toolchain file include

include(${NDK_TOOLCHAIN_FILE})

# ----------------------------------------------------------------------------
# NDK toolchain file CMake variable overrides

## Android NDK YASM macOS arm64 workaround

# When cross-compiling for Android on macOS arm64, the Yasm executable bundled into
# the NDK is used as the CMAKE_ASM_NASM_COMPILER. However, this executable is only
# built for macOS x64, preventing it to run on arm64.
# See NDK Github issue: https://github.com/android/ndk/issues/1549

# Workaround this issue by overriding it with Homebrew's Yasm, which is ensured to be
# installed on the system by build_environment/cmake/check_software.cmake.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_ASM_NASM_COMPILER /opt/homebrew/bin/yasm)
endif()
