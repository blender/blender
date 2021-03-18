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

ExternalProject_Add(external_jemalloc
  URL file://${PACKAGE_DIR}/${JEMALLOC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${JEMALLOC_HASH_TYPE}=${JEMALLOC_HASH}
  PREFIX ${BUILD_DIR}/jemalloc
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/jemalloc/src/external_jemalloc/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/jemalloc --disable-shared --enable-static --with-pic
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/jemalloc/src/external_jemalloc/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/jemalloc/src/external_jemalloc/ && make install
  INSTALL_DIR ${LIBDIR}/jemalloc
)
