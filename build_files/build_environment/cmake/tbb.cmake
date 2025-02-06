# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(TBB_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=On
  -DTBBMALLOC_BUILD=On
  -DTBBMALLOC_PROXY_BUILD=On
  -DTBB_TEST=Off
  -DCMAKE_DEBUG_POSTFIX=_debug
  # Don't pick up hwloc shared library from system package manager.
  -DTBB_DISABLE_HWLOC_AUTOMATIC_SEARCH=ON
)
# TBB does not use soversion by default unlike other libs, but it's needed
# to avoid conflicts with incompatible TBB system libs in LD_LIBRARY_PATH
# or the Steam environment.
if(UNIX AND NOT APPLE)
  list(APPEND TBB_EXTRA_ARGS -DTBB_SET_SOVERSION=ON)
endif()
set(TBB_LIBRARY tbb)
set(TBB_STATIC_LIBRARY Off)

# CMake script for TBB from https://github.com/wjakob/tbb/blob/master/CMakeLists.txt
ExternalProject_Add(external_tbb
  URL file://${PACKAGE_DIR}/${TBB_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${TBB_HASH_TYPE}=${TBB_HASH}
  PREFIX ${BUILD_DIR}/tbb
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/tbb/src/external_tbb <
      ${PATCH_DIR}/tbb_1478.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tbb ${DEFAULT_CMAKE_FLAGS} ${TBB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_tbb after_install
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/tbb/
        ${HARVEST_TARGET}/tbb/
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_tbb after_install
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/tbb/lib/
        ${HARVEST_TARGET}/tbb/lib/
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/tbb/bin/
        ${HARVEST_TARGET}/tbb/bin/
      DEPENDEES install
    )
  endif()
else()
  harvest(external_tbb tbb/include tbb/include "*.h")
  harvest_rpath_lib(external_tbb tbb/lib tbb/lib "*${SHAREDLIBEXT}*")
endif()
