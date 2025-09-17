# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ZSTD_EXTRA_ARGS
  -DZSTD_BUILD_PROGRAMS=OFF
  -DZSTD_BUILD_SHARED=OFF
  -DZSTD_BUILD_STATIC=ON
  -DZSTD_BUILD_TESTS=OFF
  -DZSTD_LEGACY_SUPPORT=OFF
  -DZSTD_LZ4_SUPPORT=OFF
  -DZSTD_LZMA_SUPPORT=OFF
  -DZSTD_MULTITHREAD_SUPPORT=ON
  -DZSTD_PROGRAMS_LINK_SHARED=OFF
  -DZSTD_USE_STATIC_RUNTIME=OFF
  -DZSTD_ZLIB_SUPPORT=OFF
)

ExternalProject_Add(external_zstd
  URL file://${PACKAGE_DIR}/${ZSTD_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ZSTD_HASH_TYPE}=${ZSTD_HASH}
  PREFIX ${BUILD_DIR}/zstd
  SOURCE_SUBDIR build/cmake

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zstd
    ${DEFAULT_CMAKE_FLAGS}
    ${ZSTD_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/zstd
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_zstd after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/zstd/lib/zstd_static${LIBEXT}
        ${HARVEST_TARGET}/zstd/lib/zstd_static${LIBEXT}
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/zstd/include/
        ${HARVEST_TARGET}/zstd/include/
      # The zstandard python extension hardcoded links to ztsd.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/zstd/lib/zstd_static${LIBEXT}
        ${LIBDIR}/zstd/lib/zstd${LIBEXT}
      DEPENDEES install
    )
  else()
    ExternalProject_Add_Step(external_zstd after_install
      # The zstandard python extension hardcoded links to ztsd.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/zstd/lib/zstd_static${LIBEXT}
        ${LIBDIR}/zstd/lib/zstd${LIBEXT}
      DEPENDEES install
    )
  endif()
else()
  harvest(external_zstd zstd/include zstd/include "*.h")
  harvest(external_zstd zstd/lib zstd/lib "*.a")
endif()
