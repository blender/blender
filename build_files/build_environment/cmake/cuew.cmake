# SPDX-License-Identifier: GPL-2.0-or-later

set(CUEW_EXTRA_ARGS)

ExternalProject_Add(external_cuew
  URL file://${PACKAGE_DIR}/${CUEW_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${CUEW_HASH_TYPE}=${CUEW_HASH}
  PREFIX ${BUILD_DIR}/cuew
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/cuew/src/external_cuew < ${PATCH_DIR}/cuew.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/cuew -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${CUEW_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/cuew
)
