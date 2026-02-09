# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(OPENJPH_EXTRA_ARGS
  -DOJPH_BUILD_TESTS=OFF
  -DOJPH_ENABLE_TIFF_SUPPORT=OFF
  -DOJPH_BUILD_EXECUTABLES=OFF
  -DBUILD_SHARED_LIBS=ON
)

ExternalProject_Add(external_openjph
  URL file://${PACKAGE_DIR}/${OPENJPH_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENJPH_HASH_TYPE}=${OPENJPH_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openjph

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openjph
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENJPH_EXTRA_ARGS}

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -N -d
      ${BUILD_DIR}/openjph/src/external_openjph/ <
      ${PATCH_DIR}/openjph_table_init_243.diff

  INSTALL_DIR ${LIBDIR}/openjph
)


if(WIN32)
  ExternalProject_Add_Step(external_openjph after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/openjph
      ${HARVEST_TARGET}/openjph

    DEPENDEES install
  )
else()
  harvest(external_openjph openjph/include openjph/include "*.h")
  # Cmake files first because harvest_rpath_lib edits them.
  harvest(external_openjph openjph/lib/cmake/openjph openjph/lib/cmake/openjph "*.cmake")
  harvest_rpath_lib(external_openjph openjph/lib openjph/lib "*${SHAREDLIBEXT}*")
endif()
