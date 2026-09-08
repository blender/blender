# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENTIMELINEIO_EXTRA_ARGS
  -DOTIO_SHARED_LIBS=ON
  -DOTIO_AUTOMATIC_SUBMODULES=OFF

  -DOTIO_CXX_INSTALL=ON
  -DOTIO_DEPENDENCIES_INSTALL=OFF

  -DOTIO_FIND_IMATH=ON
  -DOTIO_FIND_RAPIDJSON=ON

  -DImath_ROOT=${LIBDIR}/imath
  -DRapidJSON_ROOT=${LIBDIR}/rapidjson

  # NOTE: Latest OpenTimelineIO stable doesn't allow us to provide our own Pybind11 (and thus build the Python
  #       bindings). A new OTIO_FIND_PYBIND11 option was added in the latest main, can be used on upgrade.
  #-DOTIO_PYTHON_INSTALL=ON
  #-DOTIO_FIND_PYBIND11=ON
  #-Dpybind11_ROOT=${LIBDIR}/pybind11
)

ExternalProject_Add(external_opentimelineio
  URL file://${PACKAGE_DIR}/${OPENTIMELINEIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENTIMELINEIO_HASH_TYPE}=${OPENTIMELINEIO_HASH}
  PREFIX ${BUILD_DIR}/opentimelineio
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

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
  harvest(external_opentimelineio opentimelineio/share/opentimelineio opentimelineio/lib/cmake/OpenTimelineIO "*.cmake")
  harvest(external_opentimelineio opentimelineio/share/opentime opentimelineio/lib/cmake/OpenTime "*.cmake")
  harvest_rpath_lib(external_opentimelineio opentimelineio/lib opentimelineio/lib "*${SHAREDLIBEXT}*")
endif()

add_dependencies(
  external_opentimelineio
  external_imath
  external_rapidjson
  external_pybind11
)
