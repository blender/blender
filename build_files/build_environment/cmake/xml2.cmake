# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(XML2_EXTRA_ARGS
  -DLIBXML2_WITH_ZLIB=OFF
  -DLIBXML2_WITH_LZMA=OFF
  -DLIBXML2_WITH_PYTHON=OFF
  -DLIBXML2_WITH_ICONV=OFF
  -DLIBXML2_WITH_TESTS=OFF
  -DLIBXML2_WITH_PROGRAMS=OFF
  -DBUILD_SHARED_LIBS=OFF
)
ExternalProject_Add(external_xml2
  URL file://${PACKAGE_DIR}/${XML2_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${XML2_HASH_TYPE}=${XML2_HASH}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/xml2
    ${DEFAULT_CMAKE_FLAGS}
    ${XML2_EXTRA_ARGS}

  PREFIX ${BUILD_DIR}/xml2
  INSTALL_DIR ${LIBDIR}/xml2
)

if(WIN32 AND BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_xml2 after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/xml2/include
      ${HARVEST_TARGET}/xml2/include
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/xml2/lib/libxml2s.lib
      ${HARVEST_TARGET}/xml2/lib/libxml2s.lib

    DEPENDEES install
  )
endif()
