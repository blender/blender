# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENTIMELINEIO_EXTRA_ARGS
    -DKTX_FEATURE_TOOLS=OFF
    -DKTX_FEATURE_TESTS=OFF
    # Disable code with problematic license
    -DKTX_FEATURE_ETC_UNPACK=OFF
    # disable AVX2 on atscenc
    -DASTCENC_ISA_AVX2=OFF
    -DASTCENC_ISA_SSE41=ON
    -DASTCENC_ISA_SSE2=OFF
    -Dzstd_ROOT=${LIBDIR}/zstd
)


ExternalProject_Add(external_ktx
  URL file://${PACKAGE_DIR}/${KTX_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${KTX_HASH_TYPE}=${KTX_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/ktx

  PATCH_COMMAND
    # This unbundles whatever zstd they got squirreled away in their extern
    # folder and uses our copy. 
    ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/ktx/src/external_ktx
      -i ${PATCH_DIR}/ktx_zstd_dep_fix.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ktx
    ${DEFAULT_CMAKE_FLAGS} ${OPENTIMELINEIO_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/ktx
)

if(WIN32)
  ExternalProject_Add_Step(external_ktx after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/ktx/
      ${HARVEST_TARGET}/ktx

    DEPENDEES install
  )
else()
  # TODO 
endif()
