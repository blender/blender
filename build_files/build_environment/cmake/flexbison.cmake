# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(FLEXBISON_EXTRA_ARGS)

ExternalProject_Add(external_flexbison
  URL file://${PACKAGE_DIR}/${FLEXBISON_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FLEXBISON_HASH_TYPE}=${FLEXBISON_HASH}
  PREFIX ${BUILD_DIR}/flexbison
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/flexbison ${DEFAULT_CMAKE_FLAGS} ${FLEXBISON_EXTRA_ARGS}
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/flexbison/src/external_flexbison/ ${LIBDIR}/flexbison/
  INSTALL_DIR ${LIBDIR}/flexbison
)
