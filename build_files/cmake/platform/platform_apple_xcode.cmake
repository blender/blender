# SPDX-FileCopyrightText: 2016 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Xcode and system configuration for Apple.

# Detect processor architecture.
if(WITH_APPLE_CROSSPLATFORM)
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE) 
  set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "YES")

  ### Configure cross-platform parameters per platform type. ###
  # IPad: iOS arm64
  if(APPLE_TARGET_DEVICE STREQUAL "ios")
    set(CMAKE_SYSTEM_NAME "iOS" CACHE INTERNAL "" FORCE)
    set(APPLE_TARGET_IOS TRUE)

    set(APPLE_SDK_CROSSPLATFORM_NAME "iPhoneOS")
    set(APPLE_SDK_CROSSPLATFORM_NAME_LOWER "iphoneos")

    set(OSX_MIN_DEPLOYMENT_TARGET 15.00)
    set(APPLE_OS_MINVERSION_CFLAG "-miphoneos-version-min=${OSX_MIN_DEPLOYMENT_TARGET}")
  # iOS-Simulator arm64
  elseif(APPLE_TARGET_DEVICE STREQUAL "ios-simulator")
    set(CMAKE_SYSTEM_NAME "iOS" CACHE INTERNAL "" FORCE)
    set(APPLE_TARGET_IOS TRUE)

    set(APPLE_SDK_CROSSPLATFORM_NAME "iPhoneSimulator")
    set(APPLE_SDK_CROSSPLATFORM_NAME_LOWER "iphonesimulator")

    set(OSX_MIN_DEPLOYMENT_TARGET 15.00)
    set(APPLE_OS_MINVERSION_CFLAG "-miphonesimulator-version-min=${OSX_MIN_DEPLOYMENT_TARGET}")
  else()
    message(FATAL_ERROR "Unsupported APPLE_TARGET_DEVICE = ${APPLE_TARGET_DEVICE}. To add support, ensure setup parameters in platform_apple_xcode.cmake are configured. ")
    set(CMAKE_SYSTEM_NAME)
    set(APPLE_TARGET_IOS FALSE)
    set(APPLE_SDK_CROSSPLATFORM_NAME)
    set(OSX_MIN_DEPLOYMENT_TARGET)
    set(APPLE_OS_MINVERSION_CFLAG)
  endif()

else()
  # MacOS
  if(NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Detected native architecture ${ARCHITECTURE}.")
    set(CMAKE_OSX_ARCHITECTURES ${ARCHITECTURE} CACHE STRING
      "Choose the architecture you want to build Blender for: arm64 or x86_64"
      FORCE)
  endif()
  
  set(OSX_MIN_DEPLOYMENT_TARGET 11.2)
  set(APPLE_OS_MINVERSION_CFLAG " -mmacosx-version-min=${OSX_MIN_DEPLOYMENT_TARGET}")
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "${OSX_MIN_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)


# Detect developer directory. Depending on configuration this may be either
# an Xcode or Command Line Tools installation.
execute_process(
  COMMAND xcode-select --print-path
  OUTPUT_VARIABLE XCODE_DEVELOPER_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Detect Xcode version. It is provided by the Xcode generator but not
# Unix Makefiles or Ninja.
if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
  # Note that `xcodebuild -version` gives output in two lines: first line will include
  # Xcode version, second one will include build number. We are only interested in the
  # first line. Here is an example of the output:
  #   Xcode 11.4
  #   Build version 11E146
  # The expected XCODE_VERSION in this case is 11.4.
  execute_process(
    COMMAND xcodebuild -version
    OUTPUT_VARIABLE _xcode_vers_build_nr
    RESULT_VARIABLE _xcode_vers_result
    ERROR_QUIET)

  if(_xcode_vers_result EQUAL 0)
    # Convert output to a single line by replacing newlines with spaces.
    # This is needed because regex replace can not operate through the newline character
    # and applies substitutions for each individual lines.
    string(REPLACE "\n" " " _xcode_vers_build_nr_single_line "${_xcode_vers_build_nr}")
    string(REGEX REPLACE "(.*)Xcode ([0-9\\.]+).*" "\\2" XCODE_VERSION "${_xcode_vers_build_nr_single_line}")
    unset(_xcode_vers_build_nr_single_line)
  endif()

  unset(_xcode_vers_build_nr)
  unset(_xcode_vers_result)
endif()

if(XCODE_VERSION)
  # Construct SDKs path ourselves, because xcode-select path could be ambiguous.
  # Both /Applications/Xcode.app/Contents/Developer or /Applications/Xcode.app would be allowed.
  if(WITH_APPLE_CROSSPLATFORM)

    set(XCODE_PLATFORM_DIR ${XCODE_DEVELOPER_DIR}/Platforms/${APPLE_SDK_CROSSPLATFORM_NAME}.platform)
    set(XCODE_SDK_DIR ${XCODE_DEVELOPER_DIR}/Platforms/${APPLE_SDK_CROSSPLATFORM_NAME}.platform/Developer/SDKs)
    
    if(NOT DEFINED OSX_SYSTEM)
    execute_process(
        COMMAND xcodebuild -version -sdk ${APPLE_SDK_CROSSPLATFORM_NAME_LOWER} SDKVersion
        OUTPUT_VARIABLE OSX_SYSTEM
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    
    message(STATUS "--- APPLE CROSS PLATFORM BUILD -----")
    message(STATUS "Detected ${APPLE_SDK_CROSSPLATFORM_NAME} ${OSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")
    message(STATUS "${APPLE_SDK_CROSSPLATFORM_NAME} Platform DIR ${XCODE_PLATFORM_DIR}")

  else()
    # MacOS builds.
    set(XCODE_SDK_DIR ${XCODE_DEVELOPER_DIR}/Platforms/MacOSX.platform/Developer/SDKs)

    # Detect SDK version to use
    if(NOT DEFINED OSX_SYSTEM)
    execute_process(
        COMMAND xcodebuild -version -sdk macosx SDKVersion
        OUTPUT_VARIABLE OSX_SYSTEM
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    message(STATUS "--- APPLE NATIVE MACOS BUILD -----")
    message(STATUS "Detected OS X ${OSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")
  endif()

  message(STATUS "SDKs Directory: " ${XCODE_SDK_DIR})
else()
  # If no Xcode version found, try detecting command line tools.
  execute_process(
    COMMAND pkgutil --pkg-info=com.apple.pkg.CLTools_Executables
    OUTPUT_VARIABLE _cltools_pkg_info
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _cltools_pkg_info_result
    ERROR_QUIET
  )

  if(_cltools_pkg_info_result EQUAL 0)
    # Extract version.
    string(REGEX REPLACE ".*version: ([0-9]+)\\.([0-9]+).*" "\\1.\\2" XCODE_VERSION "${_cltools_pkg_info}")
    # SDK directory.
    set(XCODE_SDK_DIR "${XCODE_DEVELOPER_DIR}/SDKs")

    # Detect SDK version to use.
    if(NOT DEFINED OSX_SYSTEM)
      execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-version
        OUTPUT_VARIABLE OSX_SYSTEM
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()

    message(STATUS "Detected OS X ${OSX_SYSTEM} and Command Line Tools ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")
    message(STATUS "SDKs Directory: " ${XCODE_SDK_DIR})
  else()
    message(FATAL_ERROR "No Xcode or Command Line Tools detected")
  endif()

  unset(_cltools_pkg_info)
  unset(__cltools_pkg_info_result)
endif()

# Require a relatively recent Xcode version.
if(${XCODE_VERSION} VERSION_LESS 10.0)
  message(FATAL_ERROR "Only Xcode version 10.0 and newer is supported")
endif()

# Collect list of OSX system versions which will be used to detect path to corresponding SDK.
# Start with macOS SDK version reported by xcodebuild and include possible extra ones.
#
# The reason for need of extra ones is because it's possible that xcodebuild will report
# SDK version in the full manner (aka major.minor.patch), but the actual path will only
# include major.minor.
#
# This happens, for example, on macOS Catalina 10.15.4 and Xcode 11.4: xcodebuild on this
# system outputs "10.15.4", but the actual SDK path is MacOSX10.15.sdk.
#
# This should be safe from picking wrong SDK version because (a) xcodebuild reports full semantic
# SDK version, so such SDK does exist on the system. And if it doesn't exist with full version
# in the path, what SDK is in the major.minor folder then.
set(OSX_SDK_TEST_VERSIONS ${OSX_SYSTEM})
if(OSX_SYSTEM MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1.\\2" OSX_SYSTEM_NO_PATCH "${OSX_SYSTEM}")
  list(APPEND OSX_SDK_TEST_VERSIONS ${OSX_SYSTEM_NO_PATCH})
  unset(OSX_SYSTEM_NO_PATCH)
endif()



# Detect SDK's for other Apple Platforms.
if(WITH_APPLE_CROSSPLATFORM)

  # Loop through all possible versions and pick the first one which resolves to a valid SDK path.
  set(OSX_SDK_PATH)
  set(OSX_SDK_FOUND FALSE)
  set(OSX_SDKROOT)
  foreach(OSX_SDK_VERSION ${OSX_SDK_TEST_VERSIONS})
    set(CURRENT_OSX_SDK_PATH "${XCODE_SDK_DIR}/${APPLE_SDK_CROSSPLATFORM_NAME}${OSX_SDK_VERSION}.sdk")
    if(EXISTS ${CURRENT_OSX_SDK_PATH})
      set(OSX_SDK_PATH "${CURRENT_OSX_SDK_PATH}")
      set(OSX_SDKROOT ${APPLE_SDK_CROSSPLATFORM_NAME_LOWER}${OSX_SDK_VERSION})
      set(OSX_SDK_FOUND TRUE)
      break()
    endif()
  endforeach()
  unset(OSX_SDK_TEST_VERSIONS)

  if(NOT OSX_SDK_FOUND)
    message(FATAL_ERROR "Unable to find SDK for ${APPLE_SDK_CROSSPLATFORM_NAME} version ${OSX_SYSTEM}")
  endif()

  message(STATUS "Detected CMAKE_OSX_SYSROOT: ${OSX_SDK_PATH}")
  message(STATUS "Detected CMAKE_APPLE_CROSSPLATFORM_SDK_ROOT: ${XCODE_SDK_DIR}")

  set(APPLE_CROSSPLATFORM_TOOLCHAIN_DIR "${XCODE_DEVELOPER_DIR}/Toolchains/${CMAKE_SYSTEM_NAME}${OSX_SYSTEM}.xctoolchain")
  set(CMAKE_OSX_SYSROOT ${OSX_SDK_PATH} CACHE PATH "" FORCE)
  set(CMAKE_APPLE_CROSSPLATFORM_SDK_ROOT ${XCODE_PLATFORM_DIR} CACHE PATH "Location of the selected ${CMAKE_SYSTEM_NAME} SDK")
  unset(OSX_SDK_PATH)
  unset(OSX_SDK_FOUND)

  if(${CMAKE_GENERATOR} MATCHES "Xcode")
    # to silence sdk not found warning, just overrides CMAKE_OSX_SYSROOT
    set(CMAKE_XCODE_ATTRIBUTE_SDKROOT ${OSX_SDKROOT})
  endif()
  unset(OSX_SDKROOT)

  # Set the find root to the iOS developer roots and to user defined paths
  if (NOT DEFINED CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT)
    set (CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT "${XCODE_PLATFORM_DIR}/Developer")
  endif (NOT DEFINED CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT)
  set (CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT ${CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT} CACHE PATH "Location of ${CMAKE_SYSTEM_NAME} Platform")
  set (CMAKE_FIND_ROOT_PATH ${CMAKE_APPLE_CROSSPLATFORM_DEVELOPER_ROOT} ${CMAKE_APPLE_CROSSPLATFORM_SDK_ROOT} ${CMAKE_PREFIX_PATH} CACHE STRING  "${CMAKE_SYSTEM_NAME} find search path root")

  # default to searching for frameworks first
  set (CMAKE_FIND_FRAMEWORK FIRST)

  ## set up the default search directories for frameworks
  set (CMAKE_SYSTEM_FRAMEWORK_PATH
    ${XCODE_PLATFORM_DIR}/System/Library/Frameworks
    ${XCODE_PLATFORM_DIR}/System/Library/PrivateFrameworks
    ${XCODE_PLATFORM_DIR}/Developer/Library/Frameworks)

  message(STATUS "Detected CMAKE_SYSTEM_FRAMEWORK_PATH: ${CMAKE_SYSTEM_FRAMEWORK_PATH}")

  ## To enable cross-compiled programs, we set to NEVER to use host tools. e.g. Git, Perl
  # Note: This may possibly break some things if use of programs is inconsistent.
  set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

  # Some libraries also require macOS sdk root.
  # TOOD: Remove duplication
  set(XCODE_MACSDK_DIR ${XCODE_DEVELOPER_DIR}/Platforms/MacOSX.platform/Developer/SDKs)

  # Detect SDK version to use
  if(NOT DEFINED MACOSX_SYSTEM)
  execute_process(
      COMMAND xcodebuild -version -sdk macosx SDKVersion
      OUTPUT_VARIABLE MACOSX_SYSTEM
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  message(STATUS "Detected MACOS X ${MACOSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")

  set(MACOSX_SDK_TEST_VERSIONS ${MACOSX_SYSTEM})
  if(MACOSX_SYSTEM MATCHES "([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1.\\2" MACOSX_SYSTEM_NO_PATCH "${MACOSX_SYSTEM}")
    list(APPEND MACOSX_SDK_TEST_VERSIONS ${MACOSX_SYSTEM_NO_PATCH})
    unset(MACOSX_SYSTEM_NO_PATCH)
  endif()


  # Loop through all possible versions and pick the first one which resolves to a valid SDK path.
  set(MACOSX_SDK_PATH)
  set(MACOSX_SDK_FOUND FALSE)
  set(MACOSX_SDKROOT)
  foreach(MACOSX_SDK_VERSION ${MACOSX_SDK_TEST_VERSIONS})
    set(CURRENT_MACOSX_SDK_PATH "${XCODE_MACSDK_DIR}/MacOSX${MACOSX_SDK_VERSION}.sdk")
    if(EXISTS ${CURRENT_MACOSX_SDK_PATH})
      set(MACOSX_SDK_PATH "${CURRENT_MACOSX_SDK_PATH}")
      set(MACOSX_SDKROOT macosx${MACOSX_SDK_VERSION})
      set(MACOSX_SDK_FOUND TRUE)
      break()
    endif()
  endforeach()
  unset(MACOSX_SDK_TEST_VERSIONS)

  if(NOT MACOSX_SDK_FOUND)
    message(FATAL_ERROR "Unable to find SDK for macOS version ${MACOSX_SYSTEM}")
  endif()

  message(STATUS "Detected MACOSX_SYSROOT: ${MACOSX_SDK_PATH}")

  set(CMAKE_MACOSX_SYSROOT ${MACOSX_SDK_PATH} CACHE PATH "" FORCE)
  unset(MACOSX_SDK_PATH)
  unset(MACOSX_SDK_FOUND)

else()

  # Loop through all possible versions and pick the first one which resolves to a valid SDK path.
  set(OSX_SDK_PATH)
  set(OSX_SDK_FOUND FALSE)
  set(OSX_SDKROOT)
  foreach(OSX_SDK_VERSION ${OSX_SDK_TEST_VERSIONS})
    set(CURRENT_OSX_SDK_PATH "${XCODE_SDK_DIR}/MacOSX${OSX_SDK_VERSION}.sdk")
    if(EXISTS ${CURRENT_OSX_SDK_PATH})
      set(OSX_SDK_PATH "${CURRENT_OSX_SDK_PATH}")
      set(OSX_SDKROOT macosx${OSX_SDK_VERSION})
      set(OSX_SDK_FOUND TRUE)
      break()
    endif()
  endforeach()
  unset(OSX_SDK_TEST_VERSIONS)

  if(NOT OSX_SDK_FOUND)
    message(FATAL_ERROR "Unable to find SDK for macOS version ${OSX_SYSTEM}")
  endif()

  message(STATUS "Detected OSX_SYSROOT: ${OSX_SDK_PATH}")

  set(CMAKE_OSX_SYSROOT ${OSX_SDK_PATH} CACHE PATH "" FORCE)
  unset(OSX_SDK_PATH)
  unset(OSX_SDK_FOUND)

  if(${CMAKE_GENERATOR} MATCHES "Xcode")
    # to silence sdk not found warning, just overrides CMAKE_OSX_SYSROOT
    set(CMAKE_XCODE_ATTRIBUTE_SDKROOT ${OSX_SDKROOT})
  endif()
  unset(OSX_SDKROOT)
endif()

if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
  # Force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else (CMake bug?)
  string(APPEND CMAKE_C_FLAGS " ${APPLE_OS_MINVERSION_CFLAG}")
  string(APPEND CMAKE_CXX_FLAGS " ${APPLE_OS_MINVERSION_CFLAG}")
  add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

if(WITH_COMPILER_CCACHE)
  if(CMAKE_GENERATOR STREQUAL "Xcode")
    find_program(CCACHE_PROGRAM ccache)
    mark_as_advanced(CCACHE_PROGRAM)
    if(CCACHE_PROGRAM)
      get_filename_component(ccompiler "${CMAKE_C_COMPILER}" NAME)
      get_filename_component(cxxcompiler "${CMAKE_CXX_COMPILER}" NAME)
      # Ccache can figure out which compiler to use if it's invoked from
      # a symlink with the name of the compiler.
      # https://ccache.dev/manual/4.1.html#_run_modes
      set(_fake_compiler_dir "${CMAKE_BINARY_DIR}/ccache")
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${_fake_compiler_dir})
      set(_fake_C_COMPILER "${_fake_compiler_dir}/${ccompiler}")
      set(_fake_CXX_COMPILER "${_fake_compiler_dir}/${cxxcompiler}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CCACHE_PROGRAM}" ${_fake_C_COMPILER})
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CCACHE_PROGRAM}" ${_fake_CXX_COMPILER})
      set(CMAKE_XCODE_ATTRIBUTE_CC         ${_fake_C_COMPILER} CACHE STRING "" FORCE)
      set(CMAKE_XCODE_ATTRIBUTE_CXX        ${_fake_CXX_COMPILER} CACHE STRING "" FORCE)
      set(CMAKE_XCODE_ATTRIBUTE_LD         ${_fake_C_COMPILER} CACHE STRING "" FORCE)
      set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS ${_fake_CXX_COMPILER} CACHE STRING "" FORCE)
      unset(_fake_compiler_dir)
      unset(_fake_C_COMPILER)
      unset(_fake_CXX_COMPILER)
    else()
      message(WARNING "Ccache NOT found, disabling WITH_COMPILER_CCACHE")
      set(WITH_COMPILER_CCACHE OFF)
    endif()
  endif()
endif()
