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

ExternalProject_Add(external_opus
  URL file://${PACKAGE_DIR}/${OPUS_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPUS_HASH_TYPE}=${OPUS_HASH}
  PREFIX ${BUILD_DIR}/opus
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/opus
    --disable-shared
    --enable-static
    --with-pic
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && make install
  INSTALL_DIR ${LIBDIR}/opus
)

if(MSVC)
  set_target_properties(external_opus PROPERTIES FOLDER Mingw)
endif()
