# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(NANOBIND_EXTRA_ARGS
  -DNB_TEST=OFF
  -DPython_EXECUTABLE=${PYTHON_BINARY}
)

set(NANOBIND_PATCH
  ${CMAKE_COMMAND} -E copy_directory
    ${LIBDIR}/robinmap/include/
    ${BUILD_DIR}/nanobind/src/external_nanobind/ext/robin_map/include/
)

ExternalProject_Add(external_nanobind
  URL file://${PACKAGE_DIR}/${NANOBIND_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NANOBIND_HASH_TYPE}=${NANOBIND_HASH}
  PREFIX ${BUILD_DIR}/nanobind
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  
  PATCH_COMMAND ${NANOBIND_PATCH}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/nanobind
    ${DEFAULT_CMAKE_FLAGS}
    ${NANOBIND_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/nanobind
)

add_dependencies(
  external_nanobind
  external_robinmap
)
