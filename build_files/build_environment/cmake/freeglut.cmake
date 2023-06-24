# SPDX-FileCopyrightText: 2012-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    set(FREEGLUT_EXTRA_ARGS
      -DFREEGLUT_BUILD_SHARED_LIBS=Off
      -DFREEGLUT_BUILD_STATIC_LIBS=On
    )

    ExternalProject_Add(external_freeglut
      URL file://${PACKAGE_DIR}/${FREEGLUT_FILE}
      DOWNLOAD_DIR ${DOWNLOAD_DIR}
      URL_HASH ${FREEGLUT_HASH_TYPE}=${FREEGLUT_HASH}
      PREFIX ${BUILD_DIR}/freeglut
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/freeglut ${DEFAULT_C_FLAGS} ${DEFAULT_CXX_FLAGS} ${FREEGLUT_EXTRA_ARGS}
      INSTALL_DIR ${LIBDIR}/freeglut
    )
  endif()
endif()
