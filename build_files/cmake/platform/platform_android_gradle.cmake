# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Targets to build, package and run Blender as an Android APK via a staged Gradle project.

# Build flow:
# - The regular Blender `install` target produces a portable install tree layout in ${CMAKE_INSTALL_PREFIX}
#   containing the Android libblender.so, bundled shared library, runtime files and assets, etc...
# - `gradle_stage` ensures `install` is ran, then copies the skeleton Gradle project from release/android
#   into ${CMAKE_BINARY_DIR}/gradle (e.g. ../build_android_release/gradle) and stages/copies the
#   shared libraries and assets into it.
# - From there the Gradle project is built using the `gradle_assemble` / `android_upload` targets below.

# Main build targets (in dependent order):
# - `gradle_stage`:    Installs Blender, with the main `blender` project target freshly rebuilt,
#                      then stages the Gradle project, see platform_android_stage.cmake.
# - `gradle_assemble`: Calls Gradle in the staged Gradle project directory to assemble
#                      and sign the Blender APK.

# Utility targets:
# - `android_upload`:  Upload the resulting APK to an attached Android device via ADB. Depends on
#                      `gradle_assemble` for easy re-building and testing during development.
# - `android_run`:     Launch the Blender application main activity via ADB.
# - `android_kill`:    Force-stop the running Blender application via ADB.
# - `android_clean`:   Clear all Blender app data, wiping config and forcing re-extraction via ADB.

# -----------------------------------------------------------------------------
# APK version and paths

set(_apk_version_name "${BLENDER_VERSION}.${BLENDER_VERSION_PATCH}")
string(APPEND _apk_version_name "-${BLENDER_VERSION_CYCLE}")

# Version code integer required by Android, used in the form: 5.1.2 -> 5010200, keeping the last
# two digits for an eventual build increment field.
math(EXPR _apk_version_code
  "${BLENDER_VERSION_MAJOR} * 1000000 + ${BLENDER_VERSION_MINOR} * 10000 + ${BLENDER_VERSION_PATCH} * 100"
)

set(_gradle_source_dir "${CMAKE_SOURCE_DIR}/release/android")
set(_gradle_staging_dir "${CMAKE_BINARY_DIR}/gradle")
# Installed target version runtime directory, equivalent to TARGETDIR_VER in source/creator/CMakelists.txt.
set(_install_target_dir "${CMAKE_INSTALL_PREFIX}/${BLENDER_VERSION}")

set(_gradle_executable "${_gradle_staging_dir}/gradlew")
set(_apk_output "${_gradle_staging_dir}/app/build/outputs/apk/debug/app-debug.apk")

# -----------------------------------------------------------------------------
# Main Android APK building targets

# Ensure Blender is freshly built and installed before staging.
add_custom_target(gradle_ensure_install
  COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target install
  COMMENT "Installing and building Blender for Gradle staging"
  USES_TERMINAL VERBATIM
)

# Staging runs a standalone CMake script to be able to separately run after the `install` step,
# allowing Gradle staging to then pick from the installed files tree.
add_custom_target(gradle_stage
  COMMAND ${CMAKE_COMMAND}
          "-DGRADLE_SOURCE_DIR=${_gradle_source_dir}"
          "-DGRADLE_STAGING_DIR=${_gradle_staging_dir}"
          "-DINSTALL_TARGET_DIR=${_install_target_dir}"
          "-DANDROID_NDK_ROOT=${ANDROID_NDK_ROOT}"
          -P "${CMAKE_CURRENT_LIST_DIR}/platform_android_stage.cmake"
  COMMENT "Staging the Gradle project into ${_gradle_staging_dir}"
  USES_TERMINAL VERBATIM
)
add_dependencies(gradle_stage gradle_ensure_install)

# TODO: assembleRelease target
add_custom_target(gradle_assemble
  COMMAND "${_gradle_executable}" -p "${_gradle_staging_dir}" assembleDebug
          "-PblenderVersionName=${_apk_version_name}"
          "-PblenderVersionCode=${_apk_version_code}"
          "-PblenderMinSdk=${ANDROID_PLATFORM_LEVEL}"
  COMMAND ${CMAKE_COMMAND} -E echo "APK built: ${_apk_output}"
  COMMENT "Assembling Blender APK via Gradle"
  USES_TERMINAL VERBATIM
)
add_dependencies(gradle_assemble gradle_stage)

unset(_apk_version_name)
unset(_apk_version_code)
unset(_gradle_source_dir)
unset(_gradle_staging_dir)
unset(_gradle_executable)

# ----------------------------------------------------------------------------
# Utility targets for uploading/running the built APK via ADB

find_program(ADB_EXECUTABLE adb)
if(NOT ADB_EXECUTABLE)
  message(WARNING "ADB executable not found. android_* APK upload/management targets will not work.")
endif()

add_custom_target(android_upload
  COMMAND "${ADB_EXECUTABLE}" install -r "${_apk_output}"
  COMMENT "Uploading and installing APK to the connected device via ADB"
  USES_TERMINAL VERBATIM
)
add_dependencies(android_upload gradle_assemble)

add_custom_target(android_run
  COMMAND "${ADB_EXECUTABLE}" shell am start -n org.blender.blender/.BlenderActivity
  USES_TERMINAL VERBATIM
)

add_custom_target(android_kill
  COMMAND "${ADB_EXECUTABLE}" shell am force-stop org.blender.blender
  USES_TERMINAL VERBATIM
)

add_custom_target(android_clean
  COMMAND "${ADB_EXECUTABLE}" shell pm clear org.blender.blender
  USES_TERMINAL VERBATIM
)

unset(_apk_output)
