# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SPIRV_REFLECT_EXTRA_ARGS
  -DSPIRV_REFLECT_EXECUTABLE=OFF
  -DSPIRV_REFLECT_STATIC_LIB=ON
)

ExternalProject_Add(external_spirv_reflect
  URL file://${PACKAGE_DIR}/${SPIRV_REFLECT_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SPIRV_REFLECT_HASH_TYPE}=${SPIRV_REFLECT_HASH}
  PREFIX ${BUILD_DIR}/spirv_reflect
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/spirv_reflect/src/external_spirv_reflect <
    ${PATCH_DIR}/spirv_reflect.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/spirv_reflect
    ${DEFAULT_CMAKE_FLAGS}
    ${SPIRV_REFLECT_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/spirv_reflect
)

add_dependencies(
  external_spirv_reflect
  external_spirv_headers
)
