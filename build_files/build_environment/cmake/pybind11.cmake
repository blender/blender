# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(PYBIND11_EXTRA_ARGS
  -DPYBIND11_TEST=OFF
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
)

ExternalProject_Add(external_pybind11
  URL file://${PACKAGE_DIR}/${PYBIND11_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${PYBIND11_HASH_TYPE}=${PYBIND11_HASH}
  PREFIX ${BUILD_DIR}/pybind11
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/pybind11/src/external_pybind11 < ${PATCH_DIR}/pybind11_4761.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/pybind11 ${DEFAULT_CMAKE_FLAGS} ${PYBIND11_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/pybind11
)

add_dependencies(
  external_pybind11
  external_python
)
