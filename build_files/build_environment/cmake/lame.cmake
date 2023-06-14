# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LAME_EXTRA_ARGS)
if(MSVC)
  set(LAME_ARCH Win64)
  set (LAME_CONFIGURE echo .)
  set (LAME_BUILD cd ${BUILD_DIR}/lame/src/external_lame/ && nmake /F MakeFile.msvc MSVCVER=${LAME_ARCH} all)
  set (LAME_INSTALL cd ${BUILD_DIR}/lame/src/external_lame/ &&
                      ${CMAKE_COMMAND} -E copy include/lame.h ${LIBDIR}/lame/include/lame/lame.h &&
                      ${CMAKE_COMMAND} -E copy output/libmp3lame-static.lib ${LIBDIR}/lame/lib/mp3lame.lib )
else()
  set(LAME_CONFIGURE ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/lame --disable-shared --enable-static ${LAME_EXTRA_ARGS}
    --enable-export=full
    --with-fileio=sndfile
    --without-vorbis
    --with-pic
    --disable-mp3x
    --disable-mp3rtp
    --disable-gtktest
    --disable-frontend)
  set(LAME_BUILD ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && make -j${MAKE_THREADS})
  set(LAME_INSTALL ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lame/src/external_lame/ && make install)
endif()

ExternalProject_Add(external_lame
  URL file://${PACKAGE_DIR}/${LAME_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LAME_HASH_TYPE}=${LAME_HASH}
  PREFIX ${BUILD_DIR}/lame
  PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/lame/src/external_lame < ${PATCH_DIR}/lame.diff
  CONFIGURE_COMMAND ${LAME_CONFIGURE}
  BUILD_COMMAND ${LAME_BUILD}
  INSTALL_COMMAND ${LAME_INSTALL}
  INSTALL_DIR ${LIBDIR}/lame
)
