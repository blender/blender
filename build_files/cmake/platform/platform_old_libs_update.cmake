# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2022 Blender Foundation

# Auto update existing CMake caches for new libraries.

# Assert that `LIBDIR` is defined.
if(NOT (DEFINED LIBDIR))
  message(FATAL_ERROR "Logical error, expected 'LIBDIR' to be defined!")
endif()

# Clear cached variables whose name matches `pattern`.
function(unset_cache_variables pattern)
  get_cmake_property(_cache_variables CACHE_VARIABLES)
  foreach(_cache_variable ${_cache_variables})
    if("${_cache_variable}" MATCHES "${pattern}")
      unset(${_cache_variable} CACHE)
    endif()
  endforeach()
endfunction()

# Clear cached variables with values containing `contents`.
function(unset_cached_varables_containting contents msg)
  get_cmake_property(_cache_variables CACHE_VARIABLES)
  set(_found)
  set(_print_msg)
  foreach(_cache_variable ${_cache_variables})
    # Skip "_" prefixed variables, these are used for internal book-keeping,
    # not under user control.
    string(FIND "${_cache_variable}" "_" _found)
    if(NOT (_found EQUAL 0))
      string(FIND "${${_cache_variable}}" "${contents}" _found)
      if(NOT (_found EQUAL -1))
        if(_found)
          unset(${_cache_variable} CACHE)
          set(_print_msg ON)
        endif()
      endif()
    endif()
  endforeach()
  if(_print_msg)
    message(STATUS ${msg})
  endif()
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

if(UNIX AND (NOT APPLE) AND LIBDIR AND (EXISTS ${LIBDIR}))
  # Only search for the path if it's found on the system.
  set(_libdir_stale "/lib/linux_centos7_x86_64/")
  unset_cached_varables_containting(
    "${_libdir_stale}"
    "Auto clearing old ${_libdir_stale} paths from CMake configuration"
  )
  unset(_libdir_stale)
endif()
