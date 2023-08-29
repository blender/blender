# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_sse2neon
  URL file://${PACKAGE_DIR}/${SSE2NEON_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SSE2NEON_HASH_TYPE}=${SSE2NEON_HASH}
  PREFIX ${BUILD_DIR}/sse2neon
  CONFIGURE_COMMAND echo sse2neon - Nothing to configure
  BUILD_COMMAND echo sse2neon - nothing to build
  INSTALL_COMMAND mkdir -p ${LIBDIR}/sse2neon && cp ${BUILD_DIR}/sse2neon/src/external_sse2neon/sse2neon.h ${LIBDIR}/sse2neon
  INSTALL_DIR ${LIBDIR}/sse2neon
)
