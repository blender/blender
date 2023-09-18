# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_flex
  URL file://${PACKAGE_DIR}/${FLEX_FILE}
  URL_HASH ${FLEX_HASH_TYPE}=${FLEX_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/flex
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/flex
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && make install
  INSTALL_DIR ${LIBDIR}/flex
)
