# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_nasm
  URL file://${PACKAGE_DIR}/${NASM_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NASM_HASH_TYPE}=${NASM_HASH}
  PREFIX ${BUILD_DIR}/nasm
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/nasm/src/external_nasm < ${PATCH_DIR}/nasm.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && ./autogen.sh && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/nasm
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && make -j${MAKE_THREADS} && make manpages
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/nasm/src/external_nasm/ && make install
  INSTALL_DIR ${LIBDIR}/nasm
)

if(UNIX)
  # `touch nasm.1 ndisasm.1` helps to create the manual pages files, even when
  # local `asciidoc` and `xmlto` packages are not installed.
  ExternalProject_Add_Step(external_nasm after_configure
    COMMAND  ${CMAKE_COMMAND} -E touch ${BUILD_DIR}/nasm/src/external_nasm/nasm.1 ${BUILD_DIR}/nasm/src/external_nasm/ndisasm.1
    DEPENDEES configure
  )
endif()
