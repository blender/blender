# SPDX-FileCopyrightText: 2002-2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LIBB2_CONFIGURE_ENV echo . && ${CONFIGURE_ENV})

if(WITH_APPLE_CROSSPLATFORM)
  # Building for non-local architecture.
  set(CROSS_COMPILE_FLAGS --host=arm --enable-native=no)
else()
  set(CROSS_COMPILE_FLAGS)
endif()

ExternalProject_Add(external_libb2
  URL file://${PACKAGE_DIR}/${LIBB2_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LIBB2_HASH_TYPE}=${LIBB2_HASH}
  PREFIX ${BUILD_DIR}/libb2

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/libb2/src/external_libb2 <
    ${PATCH_DIR}/libb2_apple_ios.diff

  CONFIGURE_COMMAND ${LIBB2_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/libb2/src/external_libb2/ &&
    ./autogen.sh &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/libb2  ${CROSS_COMPILE_FLAGS}

  BUILD_COMMAND ${LIBB2_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/libb2/src/external_libb2/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${LIBB2_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/libb2/src/external_libb2/ &&
    make install

  INSTALL_DIR ${LIBDIR}/libb2
)

if(WITH_APPLE_CROSSPLATFORM)
  # Required to provide libs for IOS_PYTHON_STATIC_LIBS
  harvest_rpath_lib(external_libb2 libb2/lib libb2/lib "*.a")
endif()
