# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_amf
  URL file://${PACKAGE_DIR}/${AMF_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${AMF_HASH_TYPE}=${AMF_HASH}
  PREFIX ${BUILD_DIR}/amf

  CONFIGURE_COMMAND echo .

  BUILD_COMMAND echo .

  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${BUILD_DIR}/amf/src/external_amf/amf/public/include
    ${LIBDIR}/amf/include/AMF

  INSTALL_DIR ${LIBDIR}/amf
)
