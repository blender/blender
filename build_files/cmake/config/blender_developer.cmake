# SPDX-License-Identifier: GPL-2.0-or-later

# Configuration for developers, with faster builds, error checking and tests.
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_developer.cmake  ../blender
#

set(WITH_ASSERT_ABORT                 ON  CACHE BOOL "" FORCE)
set(WITH_BUILDINFO                    OFF CACHE BOOL "" FORCE)
# Sadly ASAN is more often broken than working with MSVC do not enable it in the
# developer profile for now.
if(NOT WIN32)
  set(WITH_COMPILER_ASAN              ON  CACHE BOOL "" FORCE)
  set(WITH_STRSIZE_DEBUG              ON  CACHE BOOL "" FORCE)
endif()
set(WITH_CYCLES_NATIVE_ONLY           ON  CACHE BOOL "" FORCE)
set(WITH_DOC_MANPAGE                  OFF CACHE BOOL "" FORCE)
set(WITH_GTESTS                       ON  CACHE BOOL "" FORCE)
set(WITH_LIBMV_SCHUR_SPECIALIZATIONS  OFF CACHE BOOL "" FORCE)
set(WITH_PYTHON_SAFETY                ON  CACHE BOOL "" FORCE)
if(WIN32)
  set(WITH_WINDOWS_BUNDLE_CRT         OFF CACHE BOOL "" FORCE)
endif()

# This may have issues with C++ initialization order, needs to be tested
# on all platforms to be sure this is safe to enable.
# set(WITH_CXX_GUARDEDALLOC             ON  CACHE BOOL "" FORCE)
