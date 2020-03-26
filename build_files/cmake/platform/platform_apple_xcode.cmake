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

if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING
    "Choose the architecture you want to build Blender for: i386, x86_64 or ppc"
    FORCE)
endif()

if(NOT DEFINED OSX_SYSTEM)
  execute_process(
      COMMAND xcodebuild -version -sdk macosx SDKVersion
      OUTPUT_VARIABLE OSX_SYSTEM
      OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# workaround for incorrect cmake xcode lookup for developer previews - XCODE_VERSION does not
# take xcode-select path into account but would always look  into /Applications/Xcode.app
# while dev versions are named Xcode<version>-DP<preview_number>
execute_process(
    COMMAND xcode-select --print-path
    OUTPUT_VARIABLE XCODE_CHECK OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "/Contents/Developer" "" XCODE_BUNDLE ${XCODE_CHECK}) # truncate to bundlepath in any case

if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
  # Unix makefile generator does not fill XCODE_VERSION var, so we get it with a command.
  # Note that `xcodebuild -version` gives output in two lines: first line will include
  # Xcode version, second one will include build number. We are only interested in the
  # former one. Here is an example of the output:
  #   Xcode 11.4
  #   Build version 11E146
  # The expected XCODE_VERSION in this case is 11.4.

  execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERS_BUILD_NR)

  # Convert output to a single line by replacling newlines with spaces.
  # This is needed because regex replace can not operate through the newline character
  # and applies substitutions for each individual lines.
  string(REPLACE "\n" " " XCODE_VERS_BUILD_NR_SINGLE_LINE "${XCODE_VERS_BUILD_NR}")

  string(REGEX REPLACE "(.*)Xcode ([0-9\\.]+).*" "\\2" XCODE_VERSION "${XCODE_VERS_BUILD_NR_SINGLE_LINE}")

  unset(XCODE_VERS_BUILD_NR)
  unset(XCODE_VERS_BUILD_NR_SINGLE_LINE)
endif()

message(STATUS "Detected OS X ${OSX_SYSTEM} and Xcode ${XCODE_VERSION} at ${XCODE_BUNDLE}")

# Older Xcode versions had different approach to the directory hiearchy.
# Require newer Xcode which is also have better chances of being able to compile with the
# required deployment target.
#
# NOTE: Xcode version 8.2 is the latest one which runs on macOS 10.11.
if(${XCODE_VERSION} VERSION_LESS 8.2)
  message(FATAL_ERROR "Only Xcode version 8.2 and newer is supported")
endif()

# note: xcode-select path could be ambiguous,
# cause /Applications/Xcode.app/Contents/Developer or /Applications/Xcode.app would be allowed
# so i use a selfcomposed bundlepath here
set(OSX_SYSROOT_PREFIX ${XCODE_BUNDLE}/Contents/Developer/Platforms/MacOSX.platform)
message(STATUS "OSX_SYSROOT_PREFIX: " ${OSX_SYSROOT_PREFIX})

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
set(OSX_SDK_PREFIX ${OSX_SYSROOT_PREFIX}/Developer/SDKs)
set(OSX_SDKROOT)
foreach(OSX_SDK_VERSION ${OSX_SDK_TEST_VERSIONS})
  set(CURRENT_OSX_SDK_PATH "${OSX_SDK_PREFIX}/MacOSX${OSX_SDK_VERSION}.sdk")
  if(EXISTS ${CURRENT_OSX_SDK_PATH})
    set(OSX_SDK_PATH "${CURRENT_OSX_SDK_PATH}")
    set(OSX_SDKROOT macosx${OSX_SDK_VERSION})
    set(OSX_SDK_FOUND TRUE)
    break()
  endif()
endforeach()
unset(OSX_SDK_PREFIX)
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

# 10.11 is our min. target, if you use higher sdk, weak linking happens
if(CMAKE_OSX_DEPLOYMENT_TARGET)
  if(${CMAKE_OSX_DEPLOYMENT_TARGET} VERSION_LESS 10.11)
    message(STATUS "Setting deployment target to 10.11, lower versions are not supported")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "" FORCE)
  endif()
else()
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "" FORCE)
endif()

if(NOT ${CMAKE_GENERATOR} MATCHES "Xcode")
  # Force CMAKE_OSX_DEPLOYMENT_TARGET for makefiles, will not work else (CMake bug?)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  add_definitions("-DMACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
