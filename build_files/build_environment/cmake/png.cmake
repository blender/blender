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

set(PNG_EXTRA_ARGS
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DPNG_STATIC=ON
)

ExternalProject_Add(external_png
  URL ${PNG_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH SHA256=${PNG_HASH}
  PREFIX ${BUILD_DIR}/png
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/png ${DEFAULT_CMAKE_FLAGS} ${PNG_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/png
)

add_dependencies(
  external_png
  external_zlib
)

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  ExternalProject_Add_Step(external_png after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/png/lib/libpng16_staticd${LIBEXT} ${LIBDIR}/png/lib/libpng16${LIBEXT}
    DEPENDEES install
  )
endif()
