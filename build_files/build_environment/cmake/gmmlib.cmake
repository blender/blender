# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(GMMLIB_EXTRA_ARGS
)

ExternalProject_Add(external_gmmlib
  URL file://${PACKAGE_DIR}/${GMMLIB_FILE}
  URL_HASH ${GMMLIB_HASH_TYPE}=${GMMLIB_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/gmmlib
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/gmmlib ${DEFAULT_CMAKE_FLAGS} ${GMMLIB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/gmmlib
)
