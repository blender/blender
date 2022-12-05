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
set(SHADERC_EXTRA_ARGS
  -DSHADERC_SKIP_TESTS=On
  -DSHADERC_SPIRV_TOOLS_DIR=${BUILD_DIR}/shaderc_spirv_tools/src/external_shaderc_spirv_tools
  -DSHADERC_SPIRV_HEADERS_DIR=${BUILD_DIR}/shaderc_spirv_headers/src/external_shaderc_spirv_headers
  -DSHADERC_GLSLANG_DIR=${BUILD_DIR}/shaderc_glslang/src/external_shaderc_glslang
  -DCMAKE_DEBUG_POSTFIX=_d
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
)

ExternalProject_Add(external_shaderc
  URL file://${PACKAGE_DIR}/${SHADERC_FILE}
  URL_HASH ${SHADERC_HASH_TYPE}=${SHADERC_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/shaderc
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/shaderc ${DEFAULT_CMAKE_FLAGS} ${SHADERC_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/shaderc
)

add_dependencies(
  external_shaderc
  external_shaderc_spirv_tools
  external_shaderc_spirv_headers
  external_shaderc_glslang
  external_python
)


if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_shaderc after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/shaderc/include ${HARVEST_TARGET}/shaderc/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/shaderc/bin/shaderc_shared.dll ${HARVEST_TARGET}/shaderc/bin/shaderc_shared.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/shaderc/lib/shaderc_shared.lib ${HARVEST_TARGET}/shaderc/lib/shaderc_shared.lib

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_shaderc after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/shaderc/bin/shaderc_shared_d.dll ${HARVEST_TARGET}/shaderc/bin/shaderc_shared_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/shaderc/lib/shaderc_shared_d.lib ${HARVEST_TARGET}/shaderc/lib/shaderc_shared_d.lib
      DEPENDEES install
    )
  endif()
endif()


