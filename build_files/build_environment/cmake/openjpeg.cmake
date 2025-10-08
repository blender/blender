# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the encoder/decoder may use png/tiff/lcms system libraries, but the
# library itself does not depend on them, so should give no problems.

if(NOT WIN32)
  set(OPENJPEG_EXTRA_ARGS ${DEFAULT_CMAKE_FLAGS})

  set(OPENJPEG_EXTRA_ARGS
    ${OPENJPEG_EXTRA_ARGS}
    -DBUILD_SHARED_LIBS=OFF
    -DBUILD_CODEC=OFF
  )

  ExternalProject_Add(external_openjpeg
    URL file://${PACKAGE_DIR}/${OPENJPEG_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${OPENJPEG_HASH_TYPE}=${OPENJPEG_HASH}
    PREFIX ${BUILD_DIR}/openjpeg

    CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build &&
      ${CMAKE_COMMAND}
        ${OPENJPEG_EXTRA_ARGS}
        -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openjpeg
        ${BUILD_DIR}/openjpeg/src/external_openjpeg

    BUILD_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build/ &&
      make -j${MAKE_THREADS}

    INSTALL_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build/ &&
      make install

    INSTALL_DIR ${LIBDIR}/openjpeg
  )

  harvest(external_openjpeg openjpeg/include/openjpeg-${OPENJPEG_SHORT_VERSION} openjpeg/include "*.h")
  harvest(external_openjpeg openjpeg/lib openjpeg/lib "*.a")
else()
  set(OPENJPEG_EXTRA_ARGS ${DEFAULT_CMAKE_FLAGS})
  ExternalProject_Add(external_openjpeg_msvc
    URL file://${PACKAGE_DIR}/${OPENJPEG_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${OPENJPEG_HASH_TYPE}=${OPENJPEG_HASH}

    PREFIX ${BUILD_DIR}/openjpeg_msvc

    CMAKE_ARGS
      ${OPENJPEG_EXTRA_ARGS}
      -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openjpeg_msvc
      -DBUILD_SHARED_LIBS=Off
      -DBUILD_THIRDPARTY=OFF

    INSTALL_DIR ${LIBDIR}/openjpeg_msvc
  )
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openjpeg_msvc after_install
      COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/openjpeg_msvc/lib
        ${HARVEST_TARGET}/openjpeg/lib &&
        ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/openjpeg_msvc/include
        ${HARVEST_TARGET}/openjpeg/include

      DEPENDEES install
    )
  endif()
endif()

set(OPENJPEG_LIBRARY libopenjp2${LIBEXT})
