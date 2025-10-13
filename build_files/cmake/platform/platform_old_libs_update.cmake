# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

# Detect update in 4.1 to shared library OpenImageDenoise and OSL.
if(UNIX AND
  DEFINED OPENIMAGEDENOISE_LIBRARY AND
  OPENIMAGEDENOISE_LIBRARY MATCHES "libOpenImageDenoise.a$" AND
  (EXISTS ${LIBDIR}/openimagedenoise/lib/libOpenImageDenoise.so OR
   EXISTS ${LIBDIR}/openimagedenoise/lib/libOpenImageDenoise.dylib))
  message(STATUS "Auto updating CMake configuration for dynamic OpenImageDenoise")
  unset_cache_variables("^OPENIMAGEDENOISE")
endif()

if(UNIX AND
  DEFINED OSL_OSLCOMP_LIBRARY AND
  OSL_OSLCOMP_LIBRARY MATCHES "liboslcomp.a$" AND
  (EXISTS ${LIBDIR}/osl/lib/liboslcomp.so OR
   EXISTS ${LIBDIR}/osl/lib/liboslcomp.dylib))
  message(STATUS "Auto updating CMake configuration for dynamic OpenShadingLanguage")
  unset_cache_variables("^OSL_")
endif()

# Detect Python 3.10 to 3.11 upgrade for Blender 4.1.
if(UNIX AND
  LIBDIR AND
  EXISTS ${LIBDIR} AND
  EXISTS ${LIBDIR}/python/bin/python3.11 AND
  DEFINED PYTHON_VERSION AND
  PYTHON_VERSION VERSION_LESS "3.11")

  message(STATUS "Auto updating CMake configuration for Python 3.11")
  unset_cache_variables("^PYTHON_")
endif()

# Detect update to 4.4 libs.
if(LIBDIR AND
   EXISTS ${LIBDIR}/tbb/include/oneapi AND
   ((DEFINED Boost_INCLUDE_DIR) OR (SYCL_LIBRARY MATCHES "sycl7")))
  message(STATUS "Auto updating CMake configuration for Blender 4.4 libraries")
  unset_cache_variables("^BOOST")
  unset_cache_variables("^Boost")
  unset_cache_variables("^EMBREE")
  unset_cache_variables("^IMATH")
  unset_cache_variables("^MATERIALX")
  unset_cache_variables("^NANOVDB")
  unset_cache_variables("^OPENCOLORIO")
  unset_cache_variables("^OPENEXR")
  unset_cache_variables("^OPENIMAGEDENOISE")
  unset_cache_variables("^OPENIMAGEIO")
  unset_cache_variables("^OPENVDB")
  unset_cache_variables("^OSL_")
  unset_cache_variables("^PYTHON")
  unset_cache_variables("^SYCL")
  unset_cache_variables("^TBB")
  unset_cache_variables("^USD")
endif()

# Detect update to 5.0 libs.
if(UNIX AND LIBDIR AND
   EXISTS ${LIBDIR}/haru/lib/libhpdf.a AND
   HARU_LIBRARY MATCHES "libhpdfs.a$")
  message(STATUS "Auto updating CMake configuration for Blender 5.0 libraries")
  unset_cache_variables("^HARU")
endif()
