# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ROBINMAP_EXTRA_ARGS
)

ExternalProject_Add(external_robinmap
  URL file://${PACKAGE_DIR}/${ROBINMAP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ROBINMAP_HASH_TYPE}=${ROBINMAP_HASH}
  PREFIX ${BUILD_DIR}/robinmap

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/robinmap
    ${DEFAULT_CMAKE_FLAGS}
    ${ROBINMAP_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/robinmap
)
