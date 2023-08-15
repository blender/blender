# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# NOTE: currently only the header file is extracted, no compilation is needed
# as the library is dynamically loaded when found on the system.

ExternalProject_Add(external_wayland_libdecor
  URL file://${PACKAGE_DIR}/${WAYLAND_LIBDECOR_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WAYLAND_LIBDECOR_HASH_TYPE}=${WAYLAND_LIBDECOR_HASH}
  PREFIX ${BUILD_DIR}/wayland_libdecor
  BUILD_COMMAND echo .
  CONFIGURE_COMMAND echo .
  INSTALL_COMMAND cp ../external_wayland_libdecor/src/libdecor.h ${LIBDIR}/wayland_libdecor/include/libdecor-0/libdecor.h
  INSTALL_DIR ${LIBDIR}/wayland_libdecor/include/libdecor-0
)
