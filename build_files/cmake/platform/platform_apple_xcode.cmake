# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# Xcode and system configuration for Apple.

# Detect processor architecture.
if(NOT CMAKE_OSX_ARCHITECTURES)
  execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
  message(STATUS "Detected native architecture ${ARCHITECTURE}.")
  set(CMAKE_OSX_ARCHITECTURES ${ARCHITECTURE} CACHE STRING
    "Choose the architecture you want to build Blender for: arm64 or x86_64"
    FORCE)
endif()

# Detect developer directory. Depending on configuration this may be either
# an Xcode or Command Line Tools installation.
execute_process(
    COMMAND xcode-select --print-path
    OUTPUT_VARIABLE XCODE_DEVELOPER_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

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
  set(XCODE_SDK_DIR ${XCODE_DEVELOPER_DIR}/Platforms/MacOSX.platform/Developer/SDKs)

  # Detect SDK version to use
  if(NOT DEFINED OSX_SYSTEM)
    execute_process(
        COMMAND xcodebuild -version -sdk macosx SDKVersion
        OUTPUT_VARIABLE OSX_SYSTEM
        OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()

  message(STATUS "Detected OS X ${OSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")
  message(STATUS "SDKs Directory: " ${XCODE_SDK_DIR})
else()
  # If no Xcode version found, try detecting command line tools.
  execute_process(
      COMMAND pkgutil --pkg-info=com.apple.pkg.CLTools_Executables
      OUTPUT_VARIABLE _cltools_pkg_info
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _cltools_pkg_info_result
      ERROR_QUIET)

  if(_cltools_pkg_info_result EQUAL 0)
    # Extract version.
    string(REGEX REPLACE ".*version: ([0-9]+)\\.([0-9]+).*" "\\1.\\2" XCODE_VERSION "${_cltools_pkg_info}")
    # SDK directory.
    set(XCODE_SDK_DIR "${XCODE_DEVELOPER_DIR}/SDKs")

    # Detect SDK version to use.
    if(NOT DEFINED OSX_SYSTEM)
      execute_process(
          COMMAND xcrun --show-sdk-version
          OUTPUT_VARIABLE OSX_SYSTEM
          OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    message(STATUS "Detected OS X ${OSX_SYSTEM} and Command Line Tools ${XCODE_VERSION} at ${XCODE_DEVELOPER_DIR}")
    message(STATUS "SDKs Directory: " ${XCODE_SDK_DIR})
  else()
    message(FATAL_ERROR "No Xcode or Command Line Tools detected")
  endif()

  unset( _cltools_pkg_info)
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


# 10.13 is our min. target, if you use higher sdk, weak linking happens
if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
  set(OSX_MIN_DEPLOYMENT_TARGET 11.00)
else()
  set(OSX_MIN_DEPLOYMENT_TARGET 10.13)
endif()

if(CMAKE_OSX_DEPLOYMENT_TARGET)
  if(${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS ${OSX_MIN_DEPLOYMENT_TARGET})
    message(STATUS "Setting deployment target to ${OSX_MIN_DEPLOYMENT_TARGET}, lower versions are not supported")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${OSX_MIN_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)
  endif()
else()
  set(CMAKE_OSX_DEPLOYMENT_TARGET "${OSX_MIN_DEPLOYMENT_TARGET}" CACHE STRING "" FORCE)
endif()

if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
  # Force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else (CMake bug?)
  string(APPEND CMAKE_C_FLAGS " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  string(APPEND CMAKE_CXX_FLAGS " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

if(WITH_COMPILER_CCACHE)
  if(CMAKE_GENERATOR STREQUAL "Xcode")
    find_program(CCACHE_PROGRAM ccache)
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
