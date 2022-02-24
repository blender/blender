# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_wayland_protocols
  URL file://${PACKAGE_DIR}/${WL_PROTOCOLS_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WL_PROTOCOLS_HASH_TYPE}=${WL_PROTOCOLS_HASH}
  PREFIX ${BUILD_DIR}/wayland-protocols
  CONFIGURE_COMMAND meson --prefix ${LIBDIR}/wayland-protocols . ../external_wayland_protocols -Dtests=false
  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
)
