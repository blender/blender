# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(PNG_EXTRA_ARGS
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DPNG_STATIC=ON
)

if(BLENDER_PLATFORM_ARM)
  set(PNG_EXTRA_ARGS
    ${PNG_EXTRA_ARGS}
    -DPNG_HARDWARE_OPTIMIZATIONS=ON
    -DPNG_ARM_NEON=on
    -DCMAKE_SYSTEM_PROCESSOR="aarch64"
  )
endif()

ExternalProject_Add(external_png
  URL file://${PACKAGE_DIR}/${PNG_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${PNG_HASH_TYPE}=${PNG_HASH}
  PREFIX ${BUILD_DIR}/png

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/png
    ${DEFAULT_CMAKE_FLAGS}
    ${PNG_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/png
)

add_dependencies(
  external_png
  external_zlib
)

if(WIN32 AND BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_png after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/png/include/
      ${HARVEST_TARGET}/png/include/
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/png/lib/libpng16_static${LIBEXT}
      ${HARVEST_TARGET}/png/lib/libpng${LIBEXT}

    DEPENDEES install
  )
endif()

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  ExternalProject_Add_Step(external_png after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/png/lib/libpng16_staticd${LIBEXT}
      ${LIBDIR}/png/lib/libpng16${LIBEXT}

    DEPENDEES install
  )
endif()
