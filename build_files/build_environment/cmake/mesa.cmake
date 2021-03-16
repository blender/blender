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

# The 'native-file', used for overrides with the meson build system.
# meson does not provide a way to do this using command line arguments.
#
# Note that we can't output to "${BUILD_DIR}/mesa/src/external_mesa" as
# it doesn't exist when CMake first executes.
file(WRITE ${BUILD_DIR}/mesa/tmp/native-file.ini "\
[binaries]
llvm-config = '${LIBDIR}/llvm/bin/llvm-config'"
)

set(MESA_EXTRA_FLAGS
  -Dbuildtype=release
  -Dc_args=${MESA_CFLAGS}
  -Dcpp_args=${MESA_CXXFLAGS}
  -Dc_link_args=${MESA_LDFLAGS}
  -Dcpp_link_args=${MESA_LDFLAGS}
  -Dglx=gallium-xlib
  -Dgallium-drivers=swrast
  -Ddri-drivers=
  -Dvulkan-drivers=
  -Dgbm=disabled
  -Degl=disabled
  -Dgles1=disabled
  -Dgles2=disabled
  -Dshared-llvm=disabled
  # Without this, the build fails when: `wayland-scanner` is not found.
  # At some point we will likely want to support Wayland.
  # Disable for now since it's not officially supported.
  -Dplatforms=x11
  --native-file ${BUILD_DIR}/mesa/tmp/native-file.ini
)

ExternalProject_Add(external_mesa
  URL file://${PACKAGE_DIR}/${MESA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${MESA_HASH_TYPE}=${MESA_HASH}
  PREFIX ${BUILD_DIR}/mesa
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/mesa/src/external_mesa/ &&
    meson ${BUILD_DIR}/mesa/src/external_mesa-build --prefix=${LIBDIR}/mesa ${MESA_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/mesa/src/external_mesa-build && ninja -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/mesa/src/external_mesa-build && ninja install
  INSTALL_DIR ${LIBDIR}/mesa
)

add_dependencies(
  external_mesa
  ll
)
