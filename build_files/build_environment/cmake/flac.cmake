# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_flac
  URL file://${PACKAGE_DIR}/${FLAC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FLAC_HASH_TYPE}=${FLAC_HASH}
  PREFIX ${BUILD_DIR}/flac
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/flac --disable-shared --enable-static
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && make install
  INSTALL_DIR ${LIBDIR}/flac
)

if(MSVC)
  set_target_properties(external_flac PROPERTIES FOLDER Mingw)
endif()
