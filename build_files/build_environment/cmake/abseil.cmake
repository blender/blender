# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ABSEIL_EXTRA_ARGS
)

if(WIN32)
  set(ABSEIL_EXTRA_ARGS
    ${ABSEIL_EXTRA_ARGS}
    -DCMAKE_DEBUG_POSTFIX=_d
  )
endif()

ExternalProject_Add(external_abseil
  URL file://${PACKAGE_DIR}/${ABSEIL_FILE}
  URL_HASH ${ABSEIL_HASH_TYPE}=${ABSEIL_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/abseil
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/abseil
    ${DEFAULT_CMAKE_FLAGS}
    ${ABSEIL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/abseil
)

ExternalProject_Add_Step(external_abseil after_install
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LIBDIR}/abseil/
    ${HARVEST_TARGET}/abseil
  COMMAND ${CMAKE_COMMAND} -E remove_directory
    ${HARVEST_TARGET}/abseil/lib/pkgconfig
  DEPENDEES install
)
