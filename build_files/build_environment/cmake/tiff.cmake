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

if(WITH_WEBP)
  set(WITH_TIFF_WEBP ON)
else()
  set(WITH_TIFF_WEBP OFF)
endif()

set(TIFF_EXTRA_ARGS
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include
  -DPNG_STATIC=ON
  -DBUILD_SHARED_LIBS=OFF
  -Dlzma=OFF
  -Djbig=OFF
  -Dzstd=OFF
  -Dwebp=${WITH_TIFF_WEBP}
)

ExternalProject_Add(external_tiff
  URL ${TIFF_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${TIFF_HASH}
  PREFIX ${BUILD_DIR}/tiff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tiff ${DEFAULT_CMAKE_FLAGS} ${TIFF_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/tiff
)

add_dependencies(
  external_tiff
  external_zlib
)

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  ExternalProject_Add_Step(external_tiff after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tiff/lib/tiffd${LIBEXT} ${LIBDIR}/tiff/lib/tiff${LIBEXT}
    DEPENDEES install
  )
endif()
