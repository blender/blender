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

set(LZMA_PATCH_CMD echo .)

ExternalProject_Add(external_lzma
  URL file://${PACKAGE_DIR}/${LZMA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LZMA_HASH_TYPE}=${LZMA_HASH}
  PREFIX ${BUILD_DIR}/lzma
  PATCH_COMMAND ${LZMA_PATCH_CMD}
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/lzma
    --disable-shared
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && make install
  INSTALL_DIR ${LIBDIR}/lzma
)
