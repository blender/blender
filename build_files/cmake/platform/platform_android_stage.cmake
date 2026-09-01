# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Stage the Gradle project and installed Blender build artifacts for APK assembly.
# Run as a script by the `gradle_stage` target, see platform/platform_android_gradle.cmake.
#
# Caller must define:
# - GRADLE_SOURCE_DIR:  Source of the Gradle project to stage (release/android)
# - GRADLE_STAGING_DIR: Directory to stage the Gradle project into.
# - INSTALL_TARGET_DIR: Installed target version runtime directory to stage.
# - ANDROID_NDK_ROOT:   Root of the Android NDK used while building.
#
# Staged into `GRADLE_STAGING_DIR` within the Gradle project itself:
# - jniLibs/arm64-v8a/: Stripped native libraries, extracted by the package installer at install
#                       time (see the useLegacyPackaging note in app/build.gradle).
# - assets/<target>/:   Runtime files, unpacked into the application's private storage on
#                       first launch by BlenderActivity.
#
# NOTE: Since this file is only ran as a CMake script, there is no need to unset variables nor prefix them with _.

foreach(required_var
  GRADLE_SOURCE_DIR
  GRADLE_STAGING_DIR
  INSTALL_TARGET_DIR
  ANDROID_NDK_ROOT
)
  if(NOT DEFINED ${required_var})
    message(FATAL_ERROR "Gradle staging: ${required_var} is not set")
  endif()
endforeach()


# -----------------------------------------------------------------------------
# Paths/File defines

get_filename_component(sdk_dir "${ANDROID_NDK_ROOT}/../.." ABSOLUTE)

set(main_dir "${GRADLE_STAGING_DIR}/app/src/main")
set(jni_dir "${main_dir}/jniLibs/arm64-v8a")
set(assets_dir "${main_dir}/assets")

# The NDK toolchain directory is named after the build host, only the one is ever installed.
file(GLOB ndk_toolchain_dirs "${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/*")
if(NOT ndk_toolchain_dirs)
  message(FATAL_ERROR "Gradle staging: No LLVM toolchain in ${ANDROID_NDK_ROOT}")
endif()
list(POP_BACK ndk_toolchain_dirs ndk_toolchain_dir)

# `libc++_shared.so` must be bundled as we used the shared STL due to the use of multiple shared libraries.
set(libcxx_shared "${ndk_toolchain_dir}/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so")
set(strip_command "${ndk_toolchain_dir}/bin/llvm-strip")

# TODO: Could add more patterns, see the source/creator install_dir macro.
set(copy_excludes
  PATTERN ".DS_Store" EXCLUDE
)

# -----------------------------------------------------------------------------
# Copy base Gradle project

# Clean to ensure no stale added/removed files linger on rebuilds, keep the base ${GRADLE_STAGING_DIR}
# to keep the .gradle folder and allow the Gradle project to be incrementally rebuilt.
file(REMOVE_RECURSE "${GRADLE_STAGING_DIR}/app/src")

# NOTE: `file(COPY)` preserves input permissions, which allows gradlew to stay executable.
file(COPY "${GRADLE_SOURCE_DIR}/" DESTINATION "${GRADLE_STAGING_DIR}" ${copy_excludes})
message(STATUS "Gradle staging: Base Gradle project staged")


# -----------------------------------------------------------------------------
# Shared native libraries

file(GLOB bundled_libs "${INSTALL_TARGET_DIR}/../lib/*.so")
set(libs ${bundled_libs} "${INSTALL_TARGET_DIR}/../libblender.so" "${libcxx_shared}")

# Strip and copy the native libraries to the staged JNI libraries directory.
file(MAKE_DIRECTORY "${jni_dir}")
foreach(src IN LISTS libs)
  if(NOT EXISTS "${src}")
    message(FATAL_ERROR "Gradle staging: Missing library ${src} (incomplete install?)")
  endif()
  get_filename_component(name "${src}" NAME)

  execute_process(
    COMMAND "${strip_command}" --strip-unneeded "${src}" -o "${jni_dir}/${name}"
    RESULT_VARIABLE strip_result
    ERROR_VARIABLE strip_error
  )
  if(NOT strip_result EQUAL 0)
    message(FATAL_ERROR "Gradle staging: Failed to strip library ${name}: ${strip_error}")
  endif()
endforeach()
message(STATUS "Gradle staging: Native libraries staged")


# -----------------------------------------------------------------------------
# Blender portable version runtime directory, staged as an APK asset

# Copy the portable target version directory tree for it to be extracted by BlenderActivity on app startup.
file(COPY "${INSTALL_TARGET_DIR}" DESTINATION "${assets_dir}/extract/" ${copy_excludes})
message(STATUS "Gradle staging: Runtime files staged")


# -----------------------------------------------------------------------------
# Gradle configuration

file(WRITE "${GRADLE_STAGING_DIR}/local.properties" "sdk.dir=${sdk_dir}\n")
