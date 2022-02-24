# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_sse2neon
  GIT_REPOSITORY  ${SSE2NEON_GIT}
  GIT_TAG ${SSE2NEON_GIT_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/sse2neon
  CONFIGURE_COMMAND echo sse2neon - Nothing to configure
  BUILD_COMMAND echo sse2neon - nothing to build
  INSTALL_COMMAND mkdir -p ${LIBDIR}/sse2neon && cp ${BUILD_DIR}/sse2neon/src/external_sse2neon/sse2neon.h ${LIBDIR}/sse2neon
  INSTALL_DIR ${LIBDIR}/sse2neon
)
