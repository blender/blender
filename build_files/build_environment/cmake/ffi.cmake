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

ExternalProject_Add(external_ffi
  URL ${FFI_URI}
  URL_HASH SHA256=${FFI_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/ffi
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ffi/src/external_ffi/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/ffi
    --enable-shared=no
    --enable-static=yes
    --with-pic
    --libdir=${LIBDIR}/ffi/lib/
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ffi/src/external_ffi/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ffi/src/external_ffi/ && make install
  PATCH_COMMAND ${PATCH_CMD} -p 0 -d ${BUILD_DIR}/ffi/src/external_ffi < ${PATCH_DIR}/ffi.diff
  INSTALL_DIR ${LIBDIR}/ffi
)

if(UNIX AND NOT APPLE)
  ExternalProject_Add_Step(external_ffi after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/ffi/lib/libffi.a ${LIBDIR}/ffi/lib/libffi_pic.a
    DEPENDEES install
  )
endif()
