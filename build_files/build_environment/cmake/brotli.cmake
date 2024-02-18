# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(BROTLI_EXTRA_ARGS
)

ExternalProject_Add(external_brotli
  URL file://${PACKAGE_DIR}/${BROTLI_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BROTLI_HASH_TYPE}=${BROTLI_HASH}
  PREFIX ${BUILD_DIR}/brotli

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/brotli
    ${DEFAULT_CMAKE_FLAGS}
    ${BROTLI_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/brotli
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_brotli after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/brotli/include
      ${HARVEST_TARGET}/brotli/include
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/brotli/lib/brotlidec-static${LIBEXT}
      ${HARVEST_TARGET}/brotli/lib/brotlidec-static${LIBEXT}
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/brotli/lib/brotlicommon-static${LIBEXT}
      ${HARVEST_TARGET}/brotli/lib/brotlicommon-static${LIBEXT}

    DEPENDEES install
  )
endif()
