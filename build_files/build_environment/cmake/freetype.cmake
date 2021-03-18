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

set(FREETYPE_EXTRA_ARGS
  -DCMAKE_RELEASE_POSTFIX:STRING=2ST
  -DCMAKE_DEBUG_POSTFIX:STRING=2ST_d
  -DWITH_BZip2=OFF
  -DWITH_HarfBuzz=OFF
  -DFT_WITH_HARFBUZZ=OFF
  -DFT_WITH_BZIP2=OFF
  -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BrotliDec=TRUE)

ExternalProject_Add(external_freetype
  URL file://${PACKAGE_DIR}/${FREETYPE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FREETYPE_HASH_TYPE}=${FREETYPE_HASH}
  PREFIX ${BUILD_DIR}/freetype
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/freetype ${DEFAULT_CMAKE_FLAGS} ${FREETYPE_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/freetype
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_freetype after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freetype ${HARVEST_TARGET}/freetype
    DEPENDEES install
  )
endif()
