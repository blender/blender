# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(CERES_EXTRA_ARGS
  -Dabsl_DIR=${LIBDIR}/abseil/lib/cmake/absl/
  -DEigen3_DIR=${LIBDIR}/eigen/share/eigen3/cmake/
  -DTBB_DIR=${LIBDIR}/tbb/lib/cmake/TBB
  -DBUILD_TESTING=OFF
  -DUSE_CUDA=OFF
  -DBUILD_EXAMPLES=OFF
  -DBUILD_SHARED_LIBS=ON
)

ExternalProject_Add(external_ceres
  URL file://${PACKAGE_DIR}/${CERES_FILE}
  URL_HASH ${CERES_HASH_TYPE}=${CERES_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/ceres
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ceres
    ${DEFAULT_CMAKE_FLAGS}
    ${CERES_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/ceres
)

add_dependencies(
  external_ceres
  external_abseil
  external_eigen
)

ExternalProject_Add_Step(external_ceres after_install
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${LIBDIR}/ceres
  ${HARVEST_TARGET}/ceres
  DEPENDEES install
)
