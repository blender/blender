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
    harvest_rpath_lib(external_openjph openjph/lib openjph/lib "*${SHAREDLIBEXT}*")
endif()
