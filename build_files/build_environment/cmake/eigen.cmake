# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(EIGEN_EXTRA_ARGS
  -DBUILD_TESTING=OFF
  -DEIGEN_BUILD_DOC=OFF
  -DTBB_DIR=${LIBDIR}/tbb/lib/cmake/TBB
)

ExternalProject_Add(external_eigen
  URL file://${PACKAGE_DIR}/${EIGEN_FILE}
  URL_HASH ${EIGEN_HASH_TYPE}=${EIGEN_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/eigen
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/eigen/src/external_eigen <
    ${PATCH_DIR}/eigen_tbb_support.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/eigen
    ${DEFAULT_CMAKE_FLAGS}
    ${EIGEN_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/eigen
)

# While C++ eigen is a header only library
if(BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_eigen after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/eigen
      ${HARVEST_TARGET}/eigen
    DEPENDEES install
  )
endif()
