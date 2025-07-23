# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LZMA_PATCH_CMD echo .)

if(WITH_APPLE_CROSSPLATFORM)
  # Building for non-local architecture.
  set(CROSS_COMPILE_FLAGS "--host=arm")
else()
  set(CROSS_COMPILE_FLAGS)
endif()

ExternalProject_Add(external_lzma
  URL file://${PACKAGE_DIR}/${LZMA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LZMA_HASH_TYPE}=${LZMA_HASH}
  PREFIX ${BUILD_DIR}/lzma
  PATCH_COMMAND ${LZMA_PATCH_CMD}

  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/lzma/src/external_lzma/ &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/lzma --disable-shared ${CROSS_COMPILE_FLAGS}

  BUILD_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/lzma/src/external_lzma/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/lzma/src/external_lzma/ &&
    make install

  INSTALL_DIR ${LIBDIR}/lzma
)

if(WITH_APPLE_CROSSPLATFORM)
  # Required to provide libs for IOS_PYTHON_STATIC_LIBS
  harvest_rpath_lib(external_lzma lzma/lib lzma/lib "*.a")
endif()
