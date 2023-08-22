# SPDX-FileCopyrightText: 2018 Blender Authors
#
# SPDX-License-Identifier: BSD-3-Clause

# - Find Embree library
# Find the native Embree includes and library
# This module defines
#  EMBREE_INCLUDE_DIRS, where to find rtcore.h, Set when
#                            EMBREE_INCLUDE_DIR is found.
#  EMBREE_LIBRARIES, libraries to link against to use Embree.
#  EMBREE_ROOT_DIR, The base directory to search for Embree.
#                        This can also be an environment variable.
#  EMBREEFOUND, If false, do not try to use Embree.

# If `EMBREE_ROOT_DIR` was defined in the environment, use it.
if(DEFINED EMBREE_ROOT_DIR)
  # Pass.
elseif(DEFINED ENV{EMBREE_ROOT_DIR})
  set(EMBREE_ROOT_DIR $ENV{EMBREE_ROOT_DIR})
else()
  set(EMBREE_ROOT_DIR "")
endif()

set(_embree_SEARCH_DIRS
  ${EMBREE_ROOT_DIR}
  /opt/lib/embree
)

find_path(EMBREE_INCLUDE_DIR
  NAMES
    embree4/rtcore.h
    embree3/rtcore.h
  HINTS
    ${_embree_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

if(EXISTS ${EMBREE_INCLUDE_DIR}/embree4/rtcore_config.h)
  set(EMBREE_MAJOR_VERSION 4)
else()
  set(EMBREE_MAJOR_VERSION 3)
endif()

if(EMBREE_INCLUDE_DIR)
  file(READ ${EMBREE_INCLUDE_DIR}/embree${EMBREE_MAJOR_VERSION}/rtcore_config.h _embree_config_header)
  if(_embree_config_header MATCHES "#define EMBREE_STATIC_LIB")
    set(EMBREE_STATIC_LIB TRUE)
  else()
    set(EMBREE_STATIC_LIB FALSE)
  endif()
  if(_embree_config_header MATCHES "#define EMBREE_SYCL_SUPPORT")
    set(EMBREE_SYCL_SUPPORT TRUE)
  else()
    set(EMBREE_SYCL_SUPPORT FALSE)
  endif()
  unset(_embree_config_header)
endif()

if(EMBREE_STATIC_LIB)
  if(NOT (("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64") OR (APPLE AND ("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64"))))
    set(_embree_SIMD_COMPONENTS
      embree_sse42
      embree_avx
      embree_avx2
    )
  endif()

  if(EMBREE_SYCL_SUPPORT)
    set(_embree_GPU_COMPONENTS
      embree4_sycl
      embree_rthwif
    )
  endif()

  set(_embree_FIND_COMPONENTS
    embree${EMBREE_MAJOR_VERSION}
    ${_embree_SIMD_COMPONENTS}
    ${_embree_GPU_COMPONENTS}
    lexers
    math
    simd
    sys
    tasking
  )
else()
  set(_embree_FIND_COMPONENTS
    embree${EMBREE_MAJOR_VERSION}
  )
  if(EMBREE_SYCL_SUPPORT)
    list(APPEND _embree_FIND_COMPONENTS
      embree4_sycl
    )
  endif()
endif()

set(_embree_LIBRARIES)
foreach(COMPONENT ${_embree_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  find_library(EMBREE_${UPPERCOMPONENT}_LIBRARY
    NAMES
      ${COMPONENT}
    HINTS
      ${_embree_SEARCH_DIRS}
    PATH_SUFFIXES
      lib64 lib
    )
  list(APPEND _embree_LIBRARIES "${EMBREE_${UPPERCOMPONENT}_LIBRARY}")
endforeach()

# handle the QUIETLY and REQUIRED arguments and set EMBREE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Embree DEFAULT_MSG
    _embree_LIBRARIES EMBREE_INCLUDE_DIR)

if(EMBREE_FOUND)
  set(EMBREE_LIBRARIES ${_embree_LIBRARIES})
  set(EMBREE_INCLUDE_DIRS ${EMBREE_INCLUDE_DIR})
endif()

mark_as_advanced(
  EMBREE_INCLUDE_DIR
  EMBREE_MAJOR_VERSION
  EMBREE_SYCL_SUPPORT
  EMBREE_STATIC_LIB
)

foreach(COMPONENT ${_embree_FIND_COMPONENTS})
  string(TOUPPER ${COMPONENT} UPPERCOMPONENT)
  mark_as_advanced(EMBREE_${UPPERCOMPONENT}_LIBRARY)
endforeach()

unset(_embree_SEARCH_DIRS)
unset(_embree_FIND_COMPONENTS)
unset(_embree_LIBRARIES)
