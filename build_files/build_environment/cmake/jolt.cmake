# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(JOLT_EXTRA_ARGS
  -DJPH_BUILD_SHARED_LIBS=ON

  -DCROSS_PLATFORM_DETERMINISTIC=ON
  -DDEBUG_RENDERER_IN_DEBUG_AND_RELEASE=OFF
  # TODO: Confirm DOUBLE_PRECISION ON/OFF value with Nodes & Physics Module developers, set to the default for now.
  -DDOUBLE_PRECISION=OFF

  # Compute backends.
  -DJPH_USE_DX12=OFF
  -DJPH_USE_MTL=ON
  -DJPH_USE_VK=ON

  # Disable unused targets.
  -DTARGET_HELLO_WORLD=OFF
  -DTARGET_PERFORMANCE_TEST=OFF
  -DTARGET_SAMPLES=OFF
  -DTARGET_UNIT_TESTS=OFF
  -DTARGET_VIEWER=OFF

  # x86_64 instruction set toggles, do not use anything above x86_64-v2 (SSE4.1/SSE4.2)
  -DUSE_SSE4_1=ON
  -DUSE_SSE4_2=ON
  -DUSE_AVX=OFF
  -DUSE_AVX2=OFF
  -DUSE_AVX512=OFF
  -DUSE_LZCNT=OFF
  -DUSE_TZCNT=OFF
  -DUSE_F16C=OFF
  -DUSE_FMADD=OFF
)


ExternalProject_Add(external_jolt
  URL file://${PACKAGE_DIR}/${JOLT_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${JOLT_HASH_TYPE}=${JOLT_HASH}
  PREFIX ${BUILD_DIR}/jolt
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  SOURCE_SUBDIR Build

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/jolt
    ${DEFAULT_CMAKE_FLAGS}
    ${JOLT_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/jolt
)

if(WIN32)
  # TODO
else()
  harvest(external_jolt jolt/include jolt/include "*.h")
  harvest(external_jolt jolt/lib/cmake/Jolt jolt/lib/cmake/Jolt "*.cmake")
  harvest_rpath_lib(external_jolt jolt/lib jolt/lib "*${SHAREDLIBEXT}*")
endif()
