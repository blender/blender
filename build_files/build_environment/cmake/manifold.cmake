# SPDX-FileCopyrightText: 2002-2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(MANIFOLD_EXTRA_ARGS
  -DMANIFOLD_JSBIND=OFF
  -DMANIFOLD_CBIND=OFF
  -DMANIFOLD_PYBIND=OFF
  -DMANIFOLD_PAR=ON
  -DMANIFOLD_CROSS_SECTION=OFF
  -DMANIFOLD_EXPORT=OFF
  -DMANIFOLD_DEBUG=OFF
  -DMANIFOLD_TEST=OFF
  -DMANIFOLD_DOWNLOADS=OFF
  -DTBB_ROOT=${LIBDIR}/tbb/
  -DTRACY_ENABLE=OFF
  -DBUILD_SHARED_LIBS=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
)

ExternalProject_Add(external_manifold
  URL file://${PACKAGE_DIR}/${MANIFOLD_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${MANIFOLD_HASH_TYPE}=${MANIFOLD_HASH}
  PREFIX ${BUILD_DIR}/manifold
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/manifold
    ${DEFAULT_CMAKE_FLAGS}
    ${MANIFOLD_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/manifold
)

add_dependencies(
  external_manifold
  external_tbb
)
if(WIN32)
  ExternalProject_Add_Step(external_manifold after_install
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/manifold/
      ${HARVEST_TARGET}/manifold/
    DEPENDEES install
  )
else()
  harvest(external_manifold manifold/include manifold/include "*.h")
  harvest(external_manifold manifold/lib manifold/lib "*.a")
  harvest(external_manifold manifold/lib/cmake/manifold manifold/lib/cmake/manifold "*.cmake")
endif()
