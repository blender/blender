# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_wayland
  URL file://${PACKAGE_DIR}/${WAYLAND_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WAYLAND_HASH_TYPE}=${WAYLAND_HASH}
  PREFIX ${BUILD_DIR}/wayland

  # Use `-E` so the `PKG_CONFIG_PATH` can be defined to link against our own LIBEXPAT/LIBXML2/FFI.
  #
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E env
      PKG_CONFIG_PATH=${LIBDIR}/expat/lib/pkgconfig:${LIBDIR}/xml2/lib/pkgconfig:${LIBDIR}/ffi/lib/pkgconfig:$PKG_CONFIG_PATH
    ${MESON}
      --prefix ${LIBDIR}/wayland
      ${MESON_BUILD_TYPE}
      -Ddocumentation=false
      -Dtests=false
      .
      ../external_wayland

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
)

add_dependencies(
  external_wayland
  external_expat
  external_xml2
  external_ffi

  # Needed for `MESON`.
  external_python_site_packages
)

harvest(external_wayland wayland/bin wayland/bin "wayland-scanner")
harvest(external_wayland wayland/include wayland/include "*.h")
# Only needed for running the WESTON compositor.
harvest(external_wayland wayland/lib64 wayland/lib64 "*")
