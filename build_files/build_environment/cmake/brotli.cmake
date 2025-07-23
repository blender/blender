# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_APPLE_CROSSPLATFORM)
  set(BROTLI_EXTRA_ARGS
  -DWITH_APPLE_CROSSPLATFORM=YES
  -DBROTLI_EXECUTABLE:string=${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/brotli/bin/brotli
  )
  set(PATCH PATCH_COMMAND ${PATCH_CMD} --verbose ${BUILD_DIR}/brotli/src/external_brotli/CMakeLists.txt ${PATCH_DIR}/brotli_ios.diff)
else()
  set(BROTLI_EXTRA_ARGS)
  set(PATCH)
endif()

ExternalProject_Add(external_brotli
  URL file://${PACKAGE_DIR}/${BROTLI_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BROTLI_HASH_TYPE}=${BROTLI_HASH}
  PREFIX ${BUILD_DIR}/brotli
  ${PATCH}
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/brotli
    ${DEFAULT_CMAKE_FLAGS}
    ${BROTLI_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/brotli
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_brotli after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/brotli/include
        ${HARVEST_TARGET}/brotli/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/brotli/lib/brotlidec-static${LIBEXT}
        ${HARVEST_TARGET}/brotli/lib/brotlidec-static${LIBEXT}
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/brotli/lib/brotlicommon-static${LIBEXT}
        ${HARVEST_TARGET}/brotli/lib/brotlicommon-static${LIBEXT}

      DEPENDEES install
    )
  endif()
else()
  harvest(external_brotli brotli/include brotli/include "*.h")
  harvest(external_brotli brotli/lib brotli/lib "*.a")
endif()
