# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_opus
  URL file://${PACKAGE_DIR}/${OPUS_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPUS_HASH_TYPE}=${OPUS_HASH}
  PREFIX ${BUILD_DIR}/opus
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/opus
    --disable-shared
    --enable-static
    --with-pic
    --disable-maintainer-mode
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/opus/src/external_opus/ && make install
  INSTALL_DIR ${LIBDIR}/opus
)

if(MSVC)
  set_target_properties(external_opus PROPERTIES FOLDER Mingw)
endif()
