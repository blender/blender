# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_ffi
  URL file://${PACKAGE_DIR}/${FFI_FILE}
  URL_HASH ${FFI_HASH_TYPE}=${FFI_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/ffi

  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/ffi
      --disable-multi-os-directory
      --enable-shared=no
      --enable-static=yes
      --with-pic
      --libdir=${LIBDIR}/ffi/lib/

  BUILD_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    make install

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    # Fix compilation errors on Apple Clang >= 17, remove when FFI is updated beyond 3.4.7, see PR #136934 for details.
    ${BUILD_DIR}/ffi/src/external_ffi <
    ${PATCH_DIR}/ffi_apple_clang_17.diff

  INSTALL_DIR ${LIBDIR}/ffi
)

if(UNIX AND NOT APPLE)
  ExternalProject_Add_Step(external_ffi after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/ffi/lib/libffi.a
      ${LIBDIR}/ffi/lib/libffi_pic.a

    DEPENDEES install
  )
endif()
