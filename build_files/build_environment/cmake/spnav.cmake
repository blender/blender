# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_spnav
  URL file://${PACKAGE_DIR}/${SPNAV_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SPNAV_HASH_TYPE}=${SPNAV_HASH}
  PREFIX ${BUILD_DIR}/spnav
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/spnav/src/external_spnav/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/spnav --disable-shared --enable-static --with-pic
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/spnav/src/external_spnav/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/spnav/src/external_spnav/ && make install
  INSTALL_DIR ${LIBDIR}/spnav
)
