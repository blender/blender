# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# These are build time requirements for shaderc. We only have to unpack these
# shaderc will build them.

ExternalProject_Add(external_shaderc_glslang
  URL file://${PACKAGE_DIR}/${SHADERC_GLSLANG_FILE}
  URL_HASH ${SHADERC_GLSLANG_HASH_TYPE}=${SHADERC_GLSLANG_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/shaderc_glslang
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)
