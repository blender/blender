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

set(LIBGLU_CFLAGS "-static-libgcc")
set(LIBGLU_CXXFLAGS "-static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a")
set(LIBGLU_LDFLAGS "-pthread -static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a")

set(LIBGLU_EXTRA_FLAGS
  CFLAGS=${LIBGLU_CFLAGS}
  CXXFLAGS=${LIBGLU_CXXFLAGS}
  LDFLAGS=${LIBGLU_LDFLAGS}
)

ExternalProject_Add(external_libglu
  URL file://${PACKAGE_DIR}/${LIBGLU_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LIBGLU_HASH_TYPE}=${LIBGLU_HASH}
  PREFIX ${BUILD_DIR}/libglu
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/libglu/src/external_libglu/ &&
    ${CONFIGURE_COMMAND_NO_TARGET} --prefix=${LIBDIR}/libglu ${LIBGLU_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/libglu/src/external_libglu/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/libglu/src/external_libglu/ && make install
  INSTALL_DIR ${LIBDIR}/libglu
)
