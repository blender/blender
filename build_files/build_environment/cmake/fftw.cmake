# SPDX-License-Identifier: GPL-2.0-or-later

set(FFTW_EXTRA_ARGS)

macro(fftw_build FFTW_POSTFIX)
  if(WIN32)
    set(FFTW3_PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX} < ${PATCH_DIR}/fftw3.diff)
    set(FFTW_EXTRA_ARGS --disable-static --enable-shared)
    set(FFTW_INSTALL install-strip)
  else()
    set(FFTW_EXTRA_ARGS --enable-static)
    set(FFTW_INSTALL install)
  endif()
  ExternalProject_Add(external_fftw3_${FFTW_POSTFIX}
    URL file://${PACKAGE_DIR}/${FFTW_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${FFTW_HASH_TYPE}=${FFTW_HASH}
    PREFIX ${BUILD_DIR}/fftw3
    CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && ${CONFIGURE_COMMAND} ${FFTW_EXTRA_ARGS} ${ARGN} --prefix=${mingw_LIBDIR}/fftw3
    PATCH_COMMAND ${FFTW3_PATCH_COMMAND}
    BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/fftw3/src/external_fftw3_${FFTW_POSTFIX}/ && make ${FFTW_INSTALL}
    INSTALL_DIR ${LIBDIR}/fftw3
  )
endmacro()

fftw_build(double)
fftw_build(float --enable-float)

if(MSVC)
  set_target_properties(external_fftw3_double PROPERTIES FOLDER Mingw)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_fftw3_double after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/libfftw3.dll.a ${HARVEST_TARGET}/fftw3/lib/libfftw3-3.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/bin/libfftw3-3.dll ${HARVEST_TARGET}/fftw3/lib/libfftw3-3.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/include/fftw3.h ${HARVEST_TARGET}/fftw3/include/fftw3.h
      DEPENDEES install
    )
    ExternalProject_Add_Step(external_fftw3_float after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/libfftw3f.dll.a ${HARVEST_TARGET}/fftw3/lib/libfftw3f.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/bin/libfftw3f-3.dll ${HARVEST_TARGET}/fftw3/lib/libfftw3f-3.dll
      DEPENDEES install
    )    
  endif()
  
endif()
