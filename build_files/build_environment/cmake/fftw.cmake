# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(FFTW_EXTRA_ARGS)

macro(fftw_build FFTW_POSTFIX)
  if(WIN32)
    if("${FFTW_POSTFIX}" STREQUAL "float")
      set(FFTW_EXTRA_ARGS -DENABLE_FLOAT=ON)
    endif()

    ExternalProject_Add(external_fftw3_${FFTW_POSTFIX}
      URL file://${PACKAGE_DIR}/${FFTW_FILE}
      DOWNLOAD_DIR ${DOWNLOAD_DIR}
      URL_HASH ${FFTW_HASH_TYPE}=${FFTW_HASH}
      PREFIX ${BUILD_DIR}/fftw3

      CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${LIBDIR}/fftw3
        -DENABLE_THREADS=ON
        -DWITH_COMBINED_THREADS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
        ${FFTW_EXTRA_ARGS}

      INSTALL_DIR ${LIBDIR}/fftw3
    )
  else()
    set(FFTW_EXTRA_ARGS --enable-static --enable-threads)
    set(FFTW_INSTALL install)
    ExternalProject_Add(external_fftw3_${FFTW_POSTFIX}
      URL file://${PACKAGE_DIR}/${FFTW_FILE}
      DOWNLOAD_DIR ${DOWNLOAD_DIR}
      URL_HASH ${FFTW_HASH_TYPE}=${FFTW_HASH}
      PREFIX ${BUILD_DIR}/fftw3

      CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
        cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ &&
        ${CONFIGURE_COMMAND} ${FFTW_EXTRA_ARGS} ${ARGN} --prefix=${mingw_LIBDIR}/fftw3

      BUILD_COMMAND ${CONFIGURE_ENV} &&
        cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ &&
        make -j${MAKE_THREADS}

      INSTALL_COMMAND ${CONFIGURE_ENV} &&
        cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ &&
        make ${FFTW_INSTALL}

      INSTALL_DIR ${LIBDIR}/fftw3
    )
  endif()
endmacro()

fftw_build(double)
fftw_build(float --enable-float)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_fftw3_double after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/fftw3/lib/fftw3.lib
        ${HARVEST_TARGET}/fftw3/lib/fftw3.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/fftw3/include/fftw3.h
        ${HARVEST_TARGET}/fftw3/include/fftw3.h
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/fftw3/lib/fftw3_threads.lib
        ${HARVEST_TARGET}/fftw3/lib/fftw3_threads.lib
      DEPENDEES install
    )
    ExternalProject_Add_Step(external_fftw3_float after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/fftw3/lib/fftw3f.lib
        ${HARVEST_TARGET}/fftw3/lib/fftw3f.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/fftw3/lib/fftw3f_threads.lib
        ${HARVEST_TARGET}/fftw3/lib/fftw3f_threads.lib
      DEPENDEES install
    )
  endif()
endif()

add_custom_target(external_fftw)
add_dependencies(
  external_fftw
  external_fftw3_double
  external_fftw3_float)

if(NOT WIN32)
  harvest(external_fftw3 fftw3/include fftw3/include "*.h")
  harvest(external_fftw3 fftw3/lib fftw3/lib "*.a")
endif()
