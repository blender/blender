# SPDX-License-Identifier: GPL-2.0-or-later

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
