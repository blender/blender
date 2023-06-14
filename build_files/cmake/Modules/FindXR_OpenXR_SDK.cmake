# SPDX-FileCopyrightText: 2020-2022 Blender Foundation
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find OpenXR-SDK libraries
# Find the native OpenXR-SDK includes and libraries
#
# Note that there is a distinction between the OpenXR standard and the SDK. The
# latter provides utilities to use the standard but is not part of it. Most
# importantly, it contains C headers and a loader library, which manages
# dynamic linking to OpenXR runtimes like Monado, Windows Mixed Reality or
# Oculus. See the repository for more details:
# https://github.com/KhronosGroup/OpenXR-SDK
#
# This module defines
#  XR_OPENXR_SDK_INCLUDE_DIRS, where to find OpenXR-SDK headers, Set when
#                           XR_OPENXR_SDK_INCLUDE_DIR is found.
#  XR_OPENXR_SDK_LIBRARIES, libraries to link against to use OpenXR.
#  XR_OPENXR_SDK_ROOT_DIR, the base directory to search for OpenXR-SDK.
#                        This can also be an environment variable.
#  XR_OPENXR_SDK_FOUND, if false, do not try to use OpenXR-SDK.
#
# also defined, but not for general use are
#  XR_OPENXR_SDK_LOADER_LIBRARY, where to find the OpenXR-SDK loader library.

# If XR_OPENXR_SDK_ROOT_DIR was defined in the environment, use it.
IF(NOT XR_OPENXR_SDK_ROOT_DIR AND NOT $ENV{XR_OPENXR_SDK_ROOT_DIR} STREQUAL "")
  SET(XR_OPENXR_SDK_ROOT_DIR $ENV{XR_OPENXR_SDK_ROOT_DIR})
ENDIF()

SET(_xr_openxr_sdk_SEARCH_DIRS
  ${XR_OPENXR_SDK_ROOT_DIR}
  /opt/lib/xr-openxr-sdk
)

FIND_PATH(XR_OPENXR_SDK_INCLUDE_DIR
  NAMES
    openxr/openxr.h
  HINTS
    ${_xr_openxr_sdk_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

FIND_LIBRARY(XR_OPENXR_SDK_LOADER_LIBRARY
  NAMES
    openxr_loader
  HINTS
    ${_xr_openxr_sdk_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
)

# handle the QUIETLY and REQUIRED arguments and set XR_OPENXR_SDK_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XR_OpenXR_SDK DEFAULT_MSG
    XR_OPENXR_SDK_LOADER_LIBRARY XR_OPENXR_SDK_INCLUDE_DIR)

IF(XR_OPENXR_SDK_FOUND)
  SET(XR_OPENXR_SDK_LIBRARIES ${XR_OPENXR_SDK_LOADER_LIBRARY})
  SET(XR_OPENXR_SDK_INCLUDE_DIRS ${XR_OPENXR_SDK_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(
  XR_OPENXR_SDK_INCLUDE_DIR
  XR_OPENXR_SDK_LOADER_LIBRARY
)
