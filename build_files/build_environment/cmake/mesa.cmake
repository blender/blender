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

set(MESA_CFLAGS "-static-libgcc")
set(MESA_CXXFLAGS "-static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a")
set(MESA_LDFLAGS "-L${LIBDIR}/zlib/lib -pthread -static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a -l:libz_pic.a")

set(MESA_EXTRA_FLAGS
  CFLAGS=${MESA_CFLAGS}
  CXXFLAGS=${MESA_CXXFLAGS}
  LDFLAGS=${MESA_LDFLAGS}
  --enable-glx=gallium-xlib
  --with-gallium-drivers=swrast
  --disable-dri
  --disable-gbm
  --disable-egl
  --disable-gles1
  --disable-gles2
  --disable-llvm-shared-libs
  --with-llvm-prefix=${LIBDIR}/llvm
)

ExternalProject_Add(external_mesa
  URL ${MESA_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${MESA_HASH}
  PREFIX ${BUILD_DIR}/mesa
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/mesa/src/external_mesa/ &&
    ${CONFIGURE_COMMAND_NO_TARGET} --prefix=${LIBDIR}/mesa ${MESA_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/mesa/src/external_mesa/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/mesa/src/external_mesa/ && make install
  INSTALL_DIR ${LIBDIR}/mesa
)

add_dependencies(
  external_mesa
  ll
)
