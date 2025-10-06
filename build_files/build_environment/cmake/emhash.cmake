# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(EMHASH_EXTRA_ARGS
  -DWITH_BENCHMARKS=OFF
)

ExternalProject_Add(external_emhash
  URL file://${PACKAGE_DIR}/${EMHASH_FILE}
  URL_HASH ${EMHASH_HASH_TYPE}=${EMHASH_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/emhash

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/emhash/src/external_emhash <
    ${PATCH_DIR}/emhash.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/emhash
    ${DEFAULT_CMAKE_FLAGS}
    ${EMHASH_EXTRA_ARGS}
)
