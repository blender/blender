# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(BZIP2_PREFIX "${LIBDIR}/bzip2")
set(BZIP2_CONFIGURATION_ARGS "")

if(UNIX AND NOT APPLE)
  set(BZIP2_LDFLAGS "-Wl,--as-needed")
  set(BZIP2_CFLAGS "-fPIC -Wall -Winline -O2 -g -D_FILE_OFFSET_BITS=64")
  if(ANDROID)
    set(BZIP2_CFLAGS "${BZIP2_CFLAGS} --target=${CMAKE_C_COMPILER_TARGET}")
  endif()
endif()

set(BZIP2_MAKE_EXTRA_FLAGS "")
set(BZIP2_MAKE_TARGET "") # Implicit `all`.
if(ANDROID)
  # Override compiler from Android CMake toolchain file.
  set(BZIP2_MAKE_EXTRA_FLAGS ${BZIP2_MAKE_FLAGS} CC=${CMAKE_C_COMPILER} AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB})

  # Skip testing phase, which would otherwise try to run the cross-compiled bzip2 executable on the wrong arch.
  set(BZIP2_MAKE_TARGET libbz2.a bzip2 bzip2recover)
endif()

ExternalProject_Add(external_bzip2
  URL file://${PACKAGE_DIR}/${BZIP2_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BZIP2_HASH_TYPE}=${BZIP2_HASH}
  PREFIX ${BUILD_DIR}/bzip2
  CONFIGURE_COMMAND echo .

  BUILD_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/bzip2/src/external_bzip2/ &&
    make CFLAGS=${BZIP2_CFLAGS} LDFLAGS=${BZIP2_LDFLAGS} ${BZIP2_MAKE_EXTRA_FLAGS} -j${MAKE_THREADS} ${BZIP2_MAKE_TARGET}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/bzip2/src/external_bzip2/ &&
    make PREFIX=${BZIP2_PREFIX} install

  INSTALL_DIR ${LIBDIR}/bzip2
)
