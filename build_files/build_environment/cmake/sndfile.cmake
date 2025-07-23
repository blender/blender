# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SNDFILE_EXTRA_ARGS
  -DCMAKE_POLICY_DEFAULT_CMP0074=NEW
  -DOgg_ROOT=${LIBDIR}/ogg
  -DVorbis_ROOT=${LIBDIR}/vorbis
  -DOpus_ROOT=${LIBDIR}/opus
  -DFLAC_ROOT=${LIBDIR}/flac
  # Use two roots because of apparent mistake in sndfile.
  -DLAME_ROOT=${LIBDIR}/lame/include
  -DMP3LAME_ROOT=${LIBDIR}/lame/lib
  -DCMAKE_DISABLE_FIND_PACKAGE_ALSA=ON
  -DCMAKE_DISABLE_FIND_PACKAGE_mpg123=ON
  -DCMAKE_DISABLE_FIND_PACKAGE_Speex=ON
  -DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON
  -DBUILD_PROGRAMS=OFF
  -DBUILD_EXAMPLES=OFF
  -DBUILD_TESTING=OFF
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
)

if(WIN32)
  # We set FLAC__NO_DLL, otherwise we cannot statically link FLAC
  set(SNDFILE_C_FLAGS "${CMAKE_C_FLAGS} -DFLAC__NO_DLL=1")
  set(SNDFILE_EXTRA_ARGS
    ${SNDFILE_EXTRA_ARGS}
    -DBUILD_SHARED_LIBS=ON
    -DCMAKE_C_FLAGS=${SNDFILE_C_FLAGS}
  )
else()
  if(WITH_APPLE_CROSSPLATFORM)
    # Building for non-local architecture.
    set(CROSS_COMPILE_FLAGS 
      --host=arm
    )

    set(EXP_OGG_LIBS -L${LIBDIR}/ogg/lib\ -logg) 
    set(EXP_OPUS_LIBS -L${LIBDIR}/opus/lib\ -lopus) 
    set(EXP_VORBIS_LIBS -L${LIBDIR}/vorbis/lib\ -lvorbis)
    set(EXP_VORBISENC_LIBS -L${LIBDIR}/vorbis/lib\ -lvorbisenc)
    set(EXP_FLAC_LIBS -L${LIBDIR}/flac/lib\ -lFLAC)
    
    set(SNDFILE_ENV
      export OGG_CFLAGS=-I"${LIBDIR}/ogg/include" &&   
      export OPUS_CFLAGS=-I"${LIBDIR}/opus/include" &&   
      export VORBIS_CFLAGS=-I"${LIBDIR}/vorbis/include" &&   
      export VORBISENC_CFLAGS=-I"${LIBDIR}/vorbis/include" &&   
      export FLAC_CFLAGS=-I"${LIBDIR}/flac/include" &&   
      export OGG_LIBS=${EXP_OGG_LIBS} &&
      export OPUS_LIBS=${EXP_OPUS_LIBS} &&
      export VORBIS_LIBS=${EXP_VORBIS_LIBS} && 
      export VORBISENC_LIBS=${EXP_VORBISENC_LIBS} &&
      export FLAC_LIBS=${EXP_FLAC_LIBS} &&
      export LIBS=${EXP_OGG_LIBS}\ ${EXP_OPUS_LIBS}\ ${EXP_VORBIS_LIBS}\ ${EXP_VORBISENC_LIBS}\ ${EXP_FLAC_LIBS} &&
    )
  else()
    set(CROSS_COMPILE_FLAGS)
  endif()

  set(SNDFILE_OPTIONS --enable-static --disable-shared ${CROSS_COMPILE_FLAGS})

  set(SNDFILE_EXTRA_ARGS
    ${SNDFILE_EXTRA_ARGS}
    -DBUILD_SHARED_LIBS=OFF
  )
endif()

ExternalProject_Add(external_sndfile
  URL file://${PACKAGE_DIR}/${SNDFILE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SNDFILE_HASH_TYPE}=${SNDFILE_HASH}
  PREFIX ${BUILD_DIR}/sndfile
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/sndfile/src/external_sndfile <
    ${PATCH_DIR}/sndfile_1045.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/sndfile
    ${DEFAULT_CMAKE_FLAGS}
    ${SNDFILE_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/sndfile
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_sndfile after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/sndfile/bin/sndfile.dll
        ${HARVEST_TARGET}/sndfile/lib/sndfile.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/sndfile/lib/sndfile.lib
        ${HARVEST_TARGET}/sndfile/lib/sndfile.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/sndfile/include/sndfile.h
        ${HARVEST_TARGET}/sndfile/include/sndfile.h

      DEPENDEES install
    )
  endif()
else()
  harvest(external_sndfile sndfile/include sndfile/include "*.h")
  harvest(external_sndfile sndfile/lib sndfile/lib "*.a")
endif()

add_dependencies(
  external_sndfile
  external_ogg
  external_vorbis
  external_opus
  external_flac
  external_lame
)
