# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OCLOC_EXTRA_ARGS
  -DNEO_SKIP_UNIT_TESTS=1
  -DNEO_BUILD_WITH_OCL=0
  -DNEO_CURRENT_PLATFORMS_SUPPORT=1
  -DNEO_LEGACY_PLATFORMS_SUPPORT=0
  -DBUILD_WITH_L0=0
  -DNEO_BUILD_UNVERSIONED_OCLOC=TRUE
  -DIGC_DIR=${LIBDIR}/igc
  -DGMM_DIR=${LIBDIR}/gmmlib
)

ExternalProject_Add(external_ocloc
  URL file://${PACKAGE_DIR}/${OCLOC_FILE}
  URL_HASH ${OCLOC_HASH_TYPE}=${OCLOC_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/ocloc

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ocloc
    ${DEFAULT_CMAKE_FLAGS}
    ${OCLOC_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/ocloc
)

add_dependencies(
  external_ocloc
  external_igc
  external_gmmlib
)

harvest(external_ocloc ocloc dpcpp/lib/ocloc "*")
