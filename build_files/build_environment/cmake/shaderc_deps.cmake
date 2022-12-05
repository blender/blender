# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

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

ExternalProject_Add(external_shaderc_spirv_headers
  URL file://${PACKAGE_DIR}/${SHADERC_SPIRV_HEADERS_FILE}
  URL_HASH ${SHADERC_SPIRV_HEADERS_HASH_TYPE}=${SHADERC_SPIRV_HEADERS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/shaderc_spirv_headers
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

ExternalProject_Add(external_shaderc_spirv_tools
  URL file://${PACKAGE_DIR}/${SHADERC_SPIRV_TOOLS_FILE}
  URL_HASH ${SHADERC_SPIRV_TOOLS_HASH_TYPE}=${SHADERC_SPIRV_TOOLS_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/shaderc_spirv_tools
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND echo .
)

