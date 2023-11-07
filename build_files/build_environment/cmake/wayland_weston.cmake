# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(WAYLAND_WESTON_CONFIGURE_ENV ${CONFIGURE_ENV})
set(WAYLAND_WESTON_PKG_ENV "PKG_CONFIG_PATH=\
${LIBDIR}/wayland/lib64/pkgconfig:\
${LIBDIR}/wayland-protocols/share/pkgconfig:\
$PKG_CONFIG_PATH"
)

ExternalProject_Add(external_wayland_weston
  URL file://${PACKAGE_DIR}/${WAYLAND_WESTON_FILE}
  URL_HASH ${WAYLAND_WESTON_HASH_TYPE}=${WAYLAND_WESTON_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/wayland_weston

  CONFIGURE_COMMAND ${WAYLAND_WESTON_CONFIGURE_ENV} &&
  ${CMAKE_COMMAND} -E env ${WAYLAND_WESTON_PKG_ENV}
  ${MESON} setup
  ${MESON_BUILD_TYPE}
  --prefix ${LIBDIR}/wayland_weston
  --libdir lib
  -Dbackend-default=headless  # For tests.
  -Dbackend-drm-screencast-vaapi=false
  -Dbackend-drm=false
  -Dbackend-pipewire=false
  -Dbackend-rdp=false
  -Dcolor-management-lcms=false
  -Ddemo-clients=false
  -Dimage-jpeg=false
  -Dimage-webp=false
  -Dpipewire=false
  -Dremoting=false
  -Dshell-fullscreen=false
  -Dshell-kiosk=false
  -Dsimple-clients=
  -Dsystemd=false
  -Dtest-junit-xml=false
  -Dwcap-decode=false
  -Dxwayland=false
  ${BUILD_DIR}/wayland_weston/src/external_wayland_weston-build
  ${BUILD_DIR}/wayland_weston/src/external_wayland_weston

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
  INSTALL_DIR ${LIBDIR}/wayland_weston
)

add_dependencies(
  external_wayland_weston
  external_wayland_protocols
  external_wayland
  # Needed for `MESON`.
  external_python_site_packages
)
