# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_wayland_protocols
  URL file://${PACKAGE_DIR}/${WL_PROTOCOLS_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WL_PROTOCOLS_HASH_TYPE}=${WL_PROTOCOLS_HASH}
  PREFIX ${BUILD_DIR}/wayland-protocols
  # Use `-E` so the `PKG_CONFIG_PATH` can be defined to link against our own WAYLAND.

  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E
    env PKG_CONFIG_PATH=${LIBDIR}/wayland/lib64/pkgconfig:$PKG_CONFIG_PATH
    ${MESON}
      --prefix ${LIBDIR}/wayland-protocols
      ${MESON_BUILD_TYPE}
      .
      ../external_wayland_protocols
      -Dtests=false

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
)

add_dependencies(
  external_wayland_protocols
  external_wayland
  # Needed for `MESON`.
  external_python_site_packages
)
