# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LEVEL_ZERO_EXTRA_ARGS
)

ExternalProject_Add(external_level-zero
  URL file://${PACKAGE_DIR}/${LEVEL_ZERO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LEVEL_ZERO_HASH_TYPE}=${LEVEL_ZERO_HASH}
  PREFIX ${BUILD_DIR}/level-zero
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/level-zero/src/external_level-zero < ${PATCH_DIR}/level-zero.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/level-zero ${DEFAULT_CMAKE_FLAGS} ${LEVEL_ZERO_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/level-zero
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_level-zero after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/level-zero ${HARVEST_TARGET}/level-zero
    DEPENDEES install
  )
endif()
