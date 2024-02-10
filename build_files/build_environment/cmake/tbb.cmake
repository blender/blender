# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(TBB_EXTRA_ARGS
  -DTBB_BUILD_SHARED=On
  -DTBB_BUILD_TBBMALLOC=On
  -DTBB_BUILD_TBBMALLOC_PROXY=On
  -DTBB_BUILD_STATIC=Off
  -DTBB_BUILD_TESTS=Off
  -DCMAKE_DEBUG_POSTFIX=_debug
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

  PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy
    ${PATCH_DIR}/cmakelists_tbb.txt
    ${BUILD_DIR}/tbb/src/external_tbb/CMakeLists.txt &&

  ${CMAKE_COMMAND} -E copy
    ${BUILD_DIR}/tbb/src/external_tbb/build/vs2013/version_string.ver
    ${BUILD_DIR}/tbb/src/external_tbb/build/version_string.ver.in &&

  ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/tbb/src/external_tbb <
    ${PATCH_DIR}/tbb.diff

  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tbb ${DEFAULT_CMAKE_FLAGS} ${TBB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_tbb after_install
      # `findtbb.cmake` in some deps *NEEDS* to find `tbb_debug.lib` even if they are not going
      # to use it to make that test pass, we place a copy with the right name in the lib folder.
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbb.lib
        ${LIBDIR}/tbb/lib/tbb_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbbmalloc.lib
        ${LIBDIR}/tbb/lib/tbbmalloc_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbb.dll
        ${LIBDIR}/tbb/bin/tbb_debug.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbbmalloc.dll
        ${LIBDIR}/tbb/bin/tbbmalloc_debug.dll
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbb.lib
        ${HARVEST_TARGET}/tbb/lib/tbb.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbb.dll
        ${HARVEST_TARGET}/tbb/bin/tbb.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbbmalloc.lib
        ${HARVEST_TARGET}/tbb/lib/tbbmalloc.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbbmalloc.dll
        ${HARVEST_TARGET}/tbb/bin/tbbmalloc.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbbmalloc_proxy.lib
        ${HARVEST_TARGET}/tbb/lib/tbbmalloc_proxy.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbbmalloc_proxy.dll
        ${HARVEST_TARGET}/tbb/bin/tbbmalloc_proxy.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/tbb/include/
        ${HARVEST_TARGET}/tbb/include/

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_tbb after_install
      # `findtbb.cmake` in some deps *NEEDS* to find `tbb.lib` even if they are not going to use
      # it to make that test pass, we place a copy with the right name in the lib folder.
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbb_debug.lib
        ${LIBDIR}/tbb/lib/tbb.lib
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbb_debug.lib
        ${HARVEST_TARGET}/tbb/lib/tbb_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbb_debug.dll
        ${HARVEST_TARGET}/tbb/bin/tbb_debug.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbbmalloc_debug.lib
        ${HARVEST_TARGET}/tbb/lib/tbbmalloc_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/lib/tbbmalloc_proxy_debug.lib
        ${HARVEST_TARGET}/tbb/lib/tbbmalloc_proxy_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbbmalloc_debug.dll
        ${HARVEST_TARGET}/tbb/bin/tbbmalloc_debug.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/tbb/bin/tbbmalloc_proxy_debug.dll
        ${HARVEST_TARGET}/tbb/bin/tbbmalloc_proxy_debug.dll
      DEPENDEES install
    )
  endif()
endif()
