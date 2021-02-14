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

ExternalProject_Add(external_nasm
  URL ${NASM_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH SHA256=${NASM_HASH}
  PREFIX ${BUILD_DIR}/nasm
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/nasm/src/external_nasm < ${PATCH_DIR}/nasm.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && ./autogen.sh && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/nasm
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && make -j${MAKE_THREADS} && make manpages
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && make install
  INSTALL_DIR ${LIBDIR}/nasm
)

if(UNIX)
  # `touch nasm.1 ndisasm.1` helps to create the manual pages files, even when
  # local `asciidoc` and `xmlto` packages are not installed.
  ExternalProject_Add_Step(external_nasm after_configure
    COMMAND  ${CMAKE_COMMAND} -E touch ${BUILD_DIR}/nasm/src/external_nasm/nasm.1 ${BUILD_DIR}/nasm/src/external_nasm/ndisasm.1
    DEPENDEES configure
  )
endif()
