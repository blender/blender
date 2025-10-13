# SPDX-FileCopyrightText: 2022-2023 Blender Authors
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

ExternalProject_Add(external_dpcpp_spirvheaders
  URL file://${PACKAGE_DIR}/${DPCPP_SPIRV_HEADERS_FILE}
  URL_HASH ${DPCPP_SPIRV_HEADERS_HASH_TYPE}=${DPCPP_SPIRV_HEADERS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/dpcpp_spirvheaders
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_unifiedruntime
  URL file://${PACKAGE_DIR}/${UNIFIED_RUNTIME_FILE}
  URL_HASH ${UNIFIED_RUNTIME_HASH_TYPE}=${UNIFIED_RUNTIME_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/unifiedruntime
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/unifiedruntime/src/external_unifiedruntime <
    ${PATCH_DIR}/unifiedruntime.diff
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_unifiedmemoryframework
  URL file://${PACKAGE_DIR}/${UNIFIED_MEMORY_FRAMEWORK_FILE}
  URL_HASH ${UNIFIED_MEMORY_FRAMEWORK_HASH_TYPE}=${UNIFIED_MEMORY_FRAMEWORK_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/unifiedmemoryframework
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/unifiedmemoryframework/src/external_unifiedmemoryframework <
    ${PATCH_DIR}/unifiedmemoryframework.diff
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)
