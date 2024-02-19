# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LZMA_PATCH_CMD echo .)

ExternalProject_Add(external_lzma
  URL file://${PACKAGE_DIR}/${LZMA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LZMA_HASH_TYPE}=${LZMA_HASH}
  PREFIX ${BUILD_DIR}/lzma
  PATCH_COMMAND ${LZMA_PATCH_CMD}
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/lzma
    --disable-shared
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lzma/src/external_lzma/ && make install
  INSTALL_DIR ${LIBDIR}/lzma
)
