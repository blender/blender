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

set(FAAD_EXTRA_ARGS)

if(WIN32)
  set(FAAD_EXTRA_CONFIGURE "utils\\win32\\ac2ver.exe" "faad2" "configure.ac" > libfaad\\win32_ver.h)
else()
  set(FAAD_EXTRA_CONFIGURE echo .)
endif()

ExternalProject_Add(external_faad
  URL ${FAAD_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${FAAD_HASH}
  PREFIX ${BUILD_DIR}/faad
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/faad/src/external_faad/ && ${FAAD_EXTRA_CONFIGURE} && ${CONFIGURE_COMMAND} --disable-shared --enable-static --prefix=${LIBDIR}/faad
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/faad/src/external_faad/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/faad/src/external_faad/ && make install
  INSTALL_DIR ${LIBDIR}/faad
)

if(MSVC)
  set_target_properties(external_faad PROPERTIES FOLDER Mingw)
endif()
