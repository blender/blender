# SPDX-FileCopyrightText: 2002-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_ogg
  URL file://${PACKAGE_DIR}/${OGG_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OGG_HASH_TYPE}=${OGG_HASH}
  PREFIX ${BUILD_DIR}/ogg
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/ogg/src/external_ogg < ${PATCH_DIR}/ogg.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ogg/src/external_ogg/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/ogg --disable-shared --enable-static
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ogg/src/external_ogg/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ogg/src/external_ogg/ && make install
  INSTALL_DIR ${LIBDIR}/ogg
)

if(MSVC)
  set_target_properties(external_ogg PROPERTIES FOLDER Mingw)
endif()
