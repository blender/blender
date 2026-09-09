# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENTIMELINEIO_EXTRA_ARGS
  -DOTIO_SHARED_LIBS=ON
  -DOTIO_AUTOMATIC_SUBMODULES=OFF
  -DBUILD_TESTING=OFF

  -DOTIO_CXX_INSTALL=ON
  -DOTIO_PYTHON_INSTALL=ON
  -DOTIO_DEPENDENCIES_INSTALL=OFF

  -DOTIO_FIND_PYBIND11=ON
  -DOTIO_FIND_IMATH=ON
  -DOTIO_FIND_RAPIDJSON=ON
  -DOTIO_FIND_MINIZIP_NG=ON

  -DImath_ROOT=${LIBDIR}/imath
  -DZLIB_ROOT=${LIBDIR}/zlib
  -DRapidJSON_ROOT=${LIBDIR}/rapidjson
  -Dminizip-ng_ROOT=${LIBDIR}/minizipng
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython_EXECUTABLE=${PYTHON_BINARY}

  # Latest main currently fails to build with C++20, try to remove on next upgrade.
  -DCMAKE_CXX_STANDARD=17
)

ExternalProject_Add(external_opentimelineio
  URL file://${PACKAGE_DIR}/${OPENTIMELINEIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENTIMELINEIO_HASH_TYPE}=${OPENTIMELINEIO_HASH}
  PREFIX ${BUILD_DIR}/opentimelineio
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND
    # This patch:
    #  - Makes install destinations relative to keep installed CMake targets relocatable
    #  - Fixes failing minizip-ng find_package() call by providing the proper minizip alias
    #  - Installs C++ libraries into lib, using @rpath install names on macOS
    #  - Omits minizip/zlib find_dependency() calls from the installed CMake package config
    ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/opentimelineio/src/external_opentimelineio
      -i ${PATCH_DIR}/opentimelineio.diff &&
    # shared support is broken and does not get CI'd upstream, small patch to fix all their issues
    ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/opentimelineio/src/external_opentimelineio
      -i ${PATCH_DIR}/opentimelineio_shared.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opentimelineio
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENTIMELINEIO_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/opentimelineio
)

if(WIN32)
  # TODO
else()
  harvest(external_opentimelineio opentimelineio/include opentimelineio/include "*.h")
  # Cmake files first because harvest_rpath_lib edits them.
  harvest(external_opentimelineio opentimelineio/lib/cmake/opentime opentimelineio/lib/cmake/opentime "*.cmake")
  harvest(external_opentimelineio opentimelineio/lib/cmake/opentimelineio opentimelineio/lib/cmake/opentimelineio "*.cmake")
  harvest_rpath_lib(external_opentimelineio opentimelineio/lib opentimelineio/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(external_opentimelineio
    opentimelineio/python/opentimelineio
    python/lib/python${PYTHON_SHORT_VERSION}/site-packages/opentimelineio
    "*"
  )
endif()

add_dependencies(
  external_opentimelineio
  external_imath
  external_rapidjson
  external_pybind11
  external_minizipng
)
