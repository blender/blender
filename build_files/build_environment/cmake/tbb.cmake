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
# ***** END GPL LICENSE BLOCK *****
if(WIN32)
  set(TBB_EXTRA_ARGS
    -DTBB_BUILD_SHARED=On
    -DTBB_BUILD_TBBMALLOC=On
    -DTBB_BUILD_TBBMALLOC_PROXY=On
    -DTBB_BUILD_STATIC=Off
  )
  set(TBB_LIBRARY tbb)
  set(TBB_STATIC_LIBRARY Off)
else()
  set(TBB_EXTRA_ARGS
    -DTBB_BUILD_SHARED=Off
    -DTBB_BUILD_TBBMALLOC=On
    -DTBB_BUILD_TBBMALLOC_PROXY=Off
    -DTBB_BUILD_STATIC=On
  )
  set(TBB_LIBRARY tbb_static)
  set(TBB_STATIC_LIBRARY On)
endif()

# CMake script for TBB from https://github.com/wjakob/tbb/blob/master/CMakeLists.txt
ExternalProject_Add(external_tbb
  URL ${TBB_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${TBB_HASH}
  PREFIX ${BUILD_DIR}/tbb
  PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_tbb.txt ${BUILD_DIR}/tbb/src/external_tbb/CMakeLists.txt &&
  ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/tbb/src/external_tbb/build/vs2013/version_string.ver ${BUILD_DIR}/tbb/src/external_tbb/src/tbb/version_string.ver
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tbb ${DEFAULT_CMAKE_FLAGS} ${TBB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_tbb after_install
      # findtbb.cmake in some deps *NEEDS* to find tbb_debug.lib even if they are not going to use it
      # to make that test pass, we place a copy with the right name in the lib folder.
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb.lib ${HARVEST_TARGET}/tbb/lib/tbb_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc.lib ${HARVEST_TARGET}/tbb/lib/tbbmalloc_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb.dll ${HARVEST_TARGET}/tbb/lib/tbb_debug.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc.dll ${HARVEST_TARGET}/tbb/lib/tbbmalloc_debug.dll
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb.lib ${HARVEST_TARGET}/tbb/lib/tbb.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb.dll ${HARVEST_TARGET}/tbb/lib/tbb.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc.lib ${HARVEST_TARGET}/tbb/lib/tbbmalloc.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc.dll ${HARVEST_TARGET}/tbb/lib/tbbmalloc.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc_proxy.lib ${HARVEST_TARGET}/tbb/lib/tbbmalloc_proxy.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc_proxy.dll ${HARVEST_TARGET}/tbb/lib/tbbmalloc_proxy.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/tbb/include/ ${HARVEST_TARGET}/tbb/include/
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_tbb after_install
      # findtbb.cmake in some deps *NEEDS* to find tbb.lib even if they are not going to use it
      # to make that test pass, we place a copy with the right name in the lib folder.
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb_debug.lib ${LIBDIR}/tbb/lib/tbb.lib
      # Normal collection of build artifacts
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb_debug.lib ${HARVEST_TARGET}/tbb/lib/debug/tbb_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb_debug.dll ${HARVEST_TARGET}/tbb/lib/debug/tbb_debug.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc_proxy.lib ${HARVEST_TARGET}/tbb/lib/tbbmalloc_proxy_debug.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc.dll ${HARVEST_TARGET}/tbb/lib/debug/tbbmalloc.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbbmalloc_proxy.dll ${HARVEST_TARGET}/tbb/lib/debug/tbbmalloc_proxy.dll
      DEPENDEES install
    )
  endif()
endif()
