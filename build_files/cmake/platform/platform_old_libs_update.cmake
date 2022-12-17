# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2022 Blender Foundation. All rights reserved.

# Auto update existing CMake caches for new libraries

function(unset_cache_variables pattern)
  get_cmake_property(_cache_variables CACHE_VARIABLES)
  foreach(_cache_variable ${_cache_variables})
    if("${_cache_variable}" MATCHES "${pattern}")
      unset(${_cache_variable} CACHE)
    endif()
  endforeach()
endfunction()

# Detect update from 3.1 to 3.2 libs.
if(UNIX AND
   DEFINED OPENEXR_VERSION AND
   OPENEXR_VERSION VERSION_LESS "3.0.0" AND
   EXISTS ${LIBDIR}/imath)
  message(STATUS "Auto updating CMake configuration for Blender 3.2 libraries")

  unset_cache_variables("^OPENIMAGEIO")
  unset_cache_variables("^OPENEXR")
  unset_cache_variables("^IMATH")
  unset_cache_variables("^PNG")
  unset_cache_variables("^USD")
  unset_cache_variables("^WEBP")
  unset_cache_variables("^NANOVDB")
endif()

# Automatically set WebP on/off depending if libraries are available.
if(EXISTS ${LIBDIR}/webp)
  if(WITH_OPENIMAGEIO)
    set(WITH_IMAGE_WEBP ON CACHE BOOL "" FORCE)
  endif()
else()
  set(WITH_IMAGE_WEBP OFF)
endif()

# NanoVDB moved into openvdb.
if(UNIX AND DEFINED NANOVDB_INCLUDE_DIR)
  if(NOT EXISTS ${NANOVDB_INCLUDE_DIR} AND
     EXISTS ${LIBDIR}/openvdb/include/nanovdb)
    unset_cache_variables("^NANOVDB")
  endif()
endif()

# Detect update to 3.5 libs with shared libraries.
if(UNIX AND
  DEFINED TBB_LIBRARY AND
  TBB_LIBRARY MATCHES "libtbb.a$" AND
  EXISTS ${LIBDIR}/usd/include/pxr/base/tf/pyModule.h)
  message(STATUS "Auto updating CMake configuration for Blender 3.5 libraries")
  unset_cache_variables("^BLOSC")
  unset_cache_variables("^BOOST")
  unset_cache_variables("^Boost")
  unset_cache_variables("^IMATH")
  unset_cache_variables("^OPENCOLORIO")
  unset_cache_variables("^OPENEXR")
  unset_cache_variables("^OPENIMAGEIO")
  unset_cache_variables("^OPENSUBDIV")
  unset_cache_variables("^OPENVDB")
  unset_cache_variables("^TBB")
  unset_cache_variables("^USD")
endif()
