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
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/fftw3 ${FFTW_EXTRA_ARGS}
      INSTALL_DIR ${LIBDIR}/fftw3
    )
  else()
    set(FFTW_EXTRA_ARGS --enable-static)
    set(FFTW_INSTALL install)
    ExternalProject_Add(external_fftw3_${FFTW_POSTFIX}
      URL file://${PACKAGE_DIR}/${FFTW_FILE}
      DOWNLOAD_DIR ${DOWNLOAD_DIR}
      URL_HASH ${FFTW_HASH_TYPE}=${FFTW_HASH}
      PREFIX ${BUILD_DIR}/fftw3
      CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && ${CONFIGURE_COMMAND} ${FFTW_EXTRA_ARGS} ${ARGN} --prefix=${mingw_LIBDIR}/fftw3
      BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && make -j${MAKE_THREADS}
      INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && make ${FFTW_INSTALL}
      INSTALL_DIR ${LIBDIR}/fftw3
    )
  endif()
endmacro()

fftw_build(double)
fftw_build(float --enable-float)

if(MSVC)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_fftw3_double after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/fftw3.lib ${HARVEST_TARGET}/fftw3/lib/fftw3.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/bin/fftw3.dll ${HARVEST_TARGET}/fftw3/lib/fftw3.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/include/fftw3.h ${HARVEST_TARGET}/fftw3/include/fftw3.h
      DEPENDEES install
    )
    ExternalProject_Add_Step(external_fftw3_float after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/fftw3f.lib ${HARVEST_TARGET}/fftw3/lib/fftw3f.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/bin/fftw3f.dll ${HARVEST_TARGET}/fftw3/lib/fftw3f.dll
      DEPENDEES install
    )    
  endif()
  
endif()
