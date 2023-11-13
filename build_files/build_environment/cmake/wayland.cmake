# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_wayland
  URL file://${PACKAGE_DIR}/${WAYLAND_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WAYLAND_HASH_TYPE}=${WAYLAND_HASH}
  PREFIX ${BUILD_DIR}/wayland
  # Use `-E` so the `PKG_CONFIG_PATH` can be defined to link against our own LIBEXPAT & LIBXML2.
  #
  # NOTE: passing link args "ffi/lib" should not be needed, but
  # `pkgconfig` would incorrectly look in "ffi/lib/../lib64" otherwise.
  #
  # NOTE: `-lm` is needed for `libxml2` which is a static library that uses `libm.so`,
  # without this, math symbols such as `floor` aren't found.
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${LIBDIR}/expat/lib/pkgconfig:${LIBDIR}/xml2/lib/pkgconfig:${LIBDIR}/ffi/lib/pkgconfig:$PKG_CONFIG_PATH
                    ${MESON} --prefix ${LIBDIR}/wayland ${MESON_BUILD_TYPE} -Ddocumentation=false -Dtests=false -D "c_link_args=-L${LIBDIR}/ffi/lib -lm" . ../external_wayland
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
