# SPDX-License-Identifier: GPL-2.0-or-later

set(LAME_EXTRA_ARGS)
if(MSVC)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
  set(LAME_EXTRA_ARGS CFLAGS=-msse)
  endif()
endif()

ExternalProject_Add(external_lame
  URL file://${PACKAGE_DIR}/${LAME_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LAME_HASH_TYPE}=${LAME_HASH}
  PREFIX ${BUILD_DIR}/lame
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/lame --disable-shared --enable-static ${LAME_EXTRA_ARGS}
    --enable-export=full
    --with-fileio=sndfile
    --without-vorbis
    --with-pic
    --disable-mp3x
    --disable-mp3rtp
    --disable-gtktest
    --disable-frontend
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && make install
  INSTALL_DIR ${LIBDIR}/lame
)

if(MSVC)
  set_target_properties(external_lame PROPERTIES FOLDER Mingw)
endif()
