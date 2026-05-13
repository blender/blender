# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(PYBIND11_EXTRA_ARGS
  -DPYBIND11_TEST=OFF
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
)
if(CMAKE_CROSSCOMPILING)
  # When cross-compiling, FindPython needs the target Python root (from LIBDIR) *and* the
  # host interpreter PYTHON_BINARY (from HOST_LIBDIR) for running scripts.
  list(APPEND PYBIND11_EXTRA_ARGS
    -DPython_ROOT_DIR=${LIBDIR}/python
  )
endif()

ExternalProject_Add(external_pybind11
  URL file://${PACKAGE_DIR}/${PYBIND11_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${PYBIND11_HASH_TYPE}=${PYBIND11_HASH}
  PREFIX ${BUILD_DIR}/pybind11
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/pybind11
    ${DEFAULT_CMAKE_FLAGS}
    ${PYBIND11_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/pybind11
)

add_dependencies(
  external_pybind11
  external_python
)
