# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
 
 set(SPIRV_TOOLS_EXTRA_ARGS
  -DSPIRV-Headers_SOURCE_DIR=${LIBDIR}/vulkan_headers
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  # both static and shared are on because the spirv-tools
  # binary are determined to link the static library and
  # will give a build error if you disable them. 
  -DBUILD_SHARED_LIBS=ON
  -DSPIRV_TOOLS_BUILD_STATIC=ON
  -DCMAKE_DEBUG_POSTFIX=_d
  -DSPIRV_SKIP_EXECUTABLES=OFF
)

ExternalProject_Add(external_spirv_tools
  URL file://${PACKAGE_DIR}/${SPIRV_TOOLS_FILE}
  URL_HASH ${SPIRV_TOOLS_HASH_TYPE}=${SPIRV_TOOLS_HASH}
  PREFIX ${BUILD_DIR}/spirv_tools
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/spirv_tools
    -Wno-dev
    ${DEFAULT_CMAKE_FLAGS}
    ${SPIRV_TOOLS_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/spirv_tools
)

add_dependencies(
  external_spirv_tools
  external_spirv_headers
  external_python
)

if(WIN32)
    ExternalProject_Add_Step(external_spirv_tools after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/spirv_tools
        ${HARVEST_TARGET}/spirv_tools

      COMMAND ${CMAKE_COMMAND}
        -DCLEAN_DIR=${HARVEST_TARGET}/spirv_tools
        -P ${CMAKE_CURRENT_LIST_DIR}/win_clean_pdb.cmake
      DEPENDEES install
    )
else()
# TODO
endif()
