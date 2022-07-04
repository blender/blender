# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_flex
  URL file://${PACKAGE_DIR}/${FLEX_FILE}
  URL_HASH ${FLEX_HASH_TYPE}=${FLEX_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/flex
  # This patch fixes build with some versions of glibc (https://github.com/westes/flex/commit/24fd0551333e7eded87b64dd36062da3df2f6380)
  PATCH_COMMAND ${PATCH_CMD} -d ${BUILD_DIR}/flex/src/external_flex < ${PATCH_DIR}/flex.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/flex
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flex/src/external_flex/ && make install
  INSTALL_DIR ${LIBDIR}/flex
)
