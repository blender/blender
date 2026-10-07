# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# These are build time requirements for shaderc. We only have to unpack these
# shaderc will build them.

set(GLSLANG_EXTRA_ARGS
  -DALLOW_EXTERNAL_SPIRV_TOOLS=ON
  -DSPIRV-Tools-opt_DIR=${LIBDIR}/spirv_tools/SPIRV-Tools-opt/cmake
  -DSPIRV-Tools_DIR=${LIBDIR}/spirv_tools/SPIRV-Tools/cmake
  -DPython3_FIND_REGISTRY=NEVER
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DBUILD_SHARED_LIBS=ON
)

ExternalProject_Add(external_glslang
  URL file://${PACKAGE_DIR}/${GLSLANG_FILE}
  URL_HASH ${GLSLANG_HASH_TYPE}=${GLSLANG_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/glslang
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/glslang
    ${DEFAULT_CMAKE_FLAGS}
    ${GLSLANG_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/glslang
)

add_dependencies(
  external_glslang
  external_spirv_tools
  external_python
)

if(WIN32)
    ExternalProject_Add_Step(external_glslang after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/glslang
        ${HARVEST_TARGET}/glslang

      COMMAND ${CMAKE_COMMAND}
        -DCLEAN_DIR=${HARVEST_TARGET}/glslang
        -P ${CMAKE_CURRENT_LIST_DIR}/win_clean_pdb.cmake
      DEPENDEES install
    )
else()
# TODO
endif()
