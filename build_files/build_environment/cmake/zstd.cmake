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

set(ZSTD_EXTRA_ARGS
  -DZSTD_BUILD_PROGRAMS=OFF
  -DZSTD_BUILD_SHARED=OFF
  -DZSTD_BUILD_STATIC=ON
  -DZSTD_BUILD_TESTS=OFF
  -DZSTD_LEGACY_SUPPORT=OFF
  -DZSTD_LZ4_SUPPORT=OFF
  -DZSTD_LZMA_SUPPORT=OFF
  -DZSTD_MULTITHREAD_SUPPORT=ON
  -DZSTD_PROGRAMS_LINK_SHARED=OFF
  -DZSTD_USE_STATIC_RUNTIME=OFF
  -DZSTD_ZLIB_SUPPORT=OFF
)

ExternalProject_Add(external_zstd
  URL file://${PACKAGE_DIR}/${ZSTD_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ZSTD_HASH_TYPE}=${ZSTD_HASH}
  PREFIX ${BUILD_DIR}/zstd
  SOURCE_SUBDIR build/cmake
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zstd ${DEFAULT_CMAKE_FLAGS} ${ZSTD_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/zstd
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_zstd after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zstd/lib/zstd_static${LIBEXT} ${HARVEST_TARGET}/zstd/lib/zstd_static${LIBEXT}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zstd/include/ ${HARVEST_TARGET}/zstd/include/
      DEPENDEES install
    )
  endif()
endif()
