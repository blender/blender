# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(BZIP2_PREFIX "${LIBDIR}/bzip2")
set(BZIP2_CONFIGURE_ENV echo .)
set(BZIP2_CONFIGURATION_ARGS)

if(UNIX AND NOT APPLE)
  set(BZIP2_LDFLAGS "-Wl,--as-needed")
  set(BZIP2_CFLAGS "-fPIC -Wall -Winline -O2 -g -D_FILE_OFFSET_BITS=64")
  set(BZIP2_CONFIGURE_ENV
    ${BZIP2_CONFIGURE_ENV} &&
    export LDFLAGS=${BZIP2_LDFLAGS} &&
    export CFLAGS=${BZIP2_CFLAGS} &&
    export PREFIX=${BZIP2_PREFIX}
  )
else()
  set(BZIP2_CONFIGURE_ENV ${CONFIGURE_ENV})
endif()

ExternalProject_Add(external_bzip2
  URL file://${PACKAGE_DIR}/${BZIP2_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BZIP2_HASH_TYPE}=${BZIP2_HASH}
  PREFIX ${BUILD_DIR}/bzip2
  CONFIGURE_COMMAND echo .

  BUILD_COMMAND ${BZIP2_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/bzip2/src/external_bzip2/ &&
    make CFLAGS=${BZIP2_CFLAGS} LDFLAGS=${BZIP2_LDFLAGS} -j${MAKE_THREADS}

  INSTALL_COMMAND ${BZIP2_CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/bzip2/src/external_bzip2/ &&
    make CFLAGS=${BZIP2_CFLAGS} LDFLAGS=${BZIP2_LDFLAGS} PREFIX=${BZIP2_PREFIX} install

  INSTALL_DIR ${LIBDIR}/bzip2
)
