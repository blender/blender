# SPDX-FileCopyrightText: 2002-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
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
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/xml2 ${DEFAULT_CMAKE_FLAGS} ${XML2_EXTRA_ARGS}
    PREFIX ${BUILD_DIR}/xml2
    INSTALL_DIR ${LIBDIR}/xml2
  )
else()
  ExternalProject_Add(external_xml2
    URL file://${PACKAGE_DIR}/${XML2_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${XML2_HASH_TYPE}=${XML2_HASH}
    PREFIX ${BUILD_DIR}/xml2
    CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && ${CONFIGURE_COMMAND}
      --prefix=${LIBDIR}/xml2
      --disable-shared
      --enable-static
      --with-pic
      --with-python=no
      --with-lzma=no
      --with-zlib=no
      --with-iconv=no
    BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && make install
    INSTALL_DIR ${LIBDIR}/xml2
  )
endif()

if(WIN32 AND BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_xml2 after_install
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/xml2/include  ${HARVEST_TARGET}/xml2/include
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/xml2/lib/libxml2s.lib  ${HARVEST_TARGET}/xml2/lib/libxml2s.lib
    DEPENDEES install
  )
endif()
