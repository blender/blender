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

set(BROTLI_EXTRA_ARGS
)

ExternalProject_Add(external_brotli
  URL file://${PACKAGE_DIR}/${BROTLI_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BROTLI_HASH_TYPE}=${BROTLI_HASH}
  PREFIX ${BUILD_DIR}/brotli
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/brotli ${DEFAULT_CMAKE_FLAGS} ${BROTLI_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/brotli
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_brotli after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/brotli/include ${HARVEST_TARGET}/brotli/include
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/brotli/lib/brotlidec-static${LIBEXT} ${HARVEST_TARGET}/brotli/lib/brotlidec-static${LIBEXT}
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/brotli/lib/brotlicommon-static${LIBEXT} ${HARVEST_TARGET}/brotli/lib/brotlicommon-static${LIBEXT}
    DEPENDEES install
  )
endif()
