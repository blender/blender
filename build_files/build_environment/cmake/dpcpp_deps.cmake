# SPDX-FileCopyrightText: 2022-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# These are build time requirements for dpcpp
# We only have to unpack these dpcpp will build
# them.

ExternalProject_Add(external_vcintrinsics
  URL file://${PACKAGE_DIR}/${VCINTRINSICS_FILE}
  URL_HASH ${VCINTRINSICS_HASH_TYPE}=${VCINTRINSICS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/vcintrinsics
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

# opencl headers do not have to be unpacked, dpcpp will do it
# but it wouldn't hurt to do it anyway as an opertunity to validate
# the hash is correct.
ExternalProject_Add(external_openclheaders
  URL file://${PACKAGE_DIR}/${OPENCLHEADERS_FILE}
  URL_HASH ${OPENCLHEADERS_HASH_TYPE}=${OPENCLHEADERS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/openclheaders
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

# icdloader does not have to be unpacked, dpcpp will do it
# but it wouldn't hurt to do it anyway as an opertunity to validate
# the hash is correct.
ExternalProject_Add(external_icdloader
  URL file://${PACKAGE_DIR}/${ICDLOADER_FILE}
  URL_HASH ${ICDLOADER_HASH_TYPE}=${ICDLOADER_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/icdloader
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_mp11
  URL file://${PACKAGE_DIR}/${MP11_FILE}
  URL_HASH ${MP11_HASH_TYPE}=${MP11_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/mp11
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_spirvheaders
  URL file://${PACKAGE_DIR}/${SPIRV_HEADERS_FILE}
  URL_HASH ${SPIRV_HEADERS_HASH_TYPE}=${SPIRV_HEADERS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/spirvheaders
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_unifiedruntime
  URL file://${PACKAGE_DIR}/${UNIFIED_RUNTIME_FILE}
  URL_HASH ${UNIFIED_RUNTIME_HASH_TYPE}=${UNIFIED_RUNTIME_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/unifiedruntime
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)
