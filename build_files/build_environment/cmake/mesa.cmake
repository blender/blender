# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
  ${MESON_BUILD_TYPE}
  -Dc_args=${MESA_CFLAGS}
  -Dcpp_args=${MESA_CXXFLAGS}
  -Dc_link_args=${MESA_LDFLAGS}
  -Dcpp_link_args=${MESA_LDFLAGS}
  -Dglx=xlib
  -Dgallium-drivers=swrast
  -Dvulkan-drivers=
  -Dgbm=disabled
  -Degl=disabled
  -Dgles1=disabled
  -Dgles2=disabled
  -Dcpp_rtti=false
  -Dshared-llvm=disabled
  # Without this, the build fails when: `wayland-scanner` is not found.
  # At some point we will likely want to support Wayland.
  # Disable for now since it's not officially supported.
  -Dplatforms=x11
  # Needed to find the local expat,zlib,zstd.
  --pkg-config-path=${LIBDIR}/expat/lib/pkgconfig,${LIBDIR}/zstd/lib/pkgconfig,${LIBDIR}/zlib/share/pkgconfig
  --native-file ${BUILD_DIR}/mesa/tmp/native-file.ini
)

ExternalProject_Add(external_mesa
  URL file://${PACKAGE_DIR}/${MESA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${MESA_HASH_TYPE}=${MESA_HASH}
  PREFIX ${BUILD_DIR}/mesa

  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/mesa/src/external_mesa/ &&
    ${MESON}
      ${BUILD_DIR}/mesa/src/external_mesa-build
      --prefix=${LIBDIR}/mesa
      ${MESA_EXTRA_FLAGS}

  BUILD_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/mesa/src/external_mesa-build &&
    ninja -j${MAKE_THREADS}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/mesa/src/external_mesa-build &&
    ninja install

  INSTALL_DIR ${LIBDIR}/mesa
)

add_dependencies(
  external_mesa
  ll
  external_zlib
  external_zstd
  # Run-time dependency.
  external_expat
  # Needed for `MESON`.
  external_python_site_packages
)

harvest(external_mesa libglu/lib mesa/lib "*${SHAREDLIBEXT}*")
harvest(external_mesa mesa/lib64 mesa/lib "*${SHAREDLIBEXT}*")
