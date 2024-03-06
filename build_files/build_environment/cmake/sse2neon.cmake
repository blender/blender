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
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/sse2neon/src/external_sse2neon/sse2neon.h ${LIBDIR}/sse2neon
  INSTALL_DIR ${LIBDIR}/sse2neon
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_sse2neon after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sse2neon ${HARVEST_TARGET}/sse2neon
    DEPENDEES install
  )
endif()
