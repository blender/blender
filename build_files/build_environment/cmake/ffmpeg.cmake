# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(temp_LIBDIR ${mingw_LIBDIR})
else()
  set(temp_LIBDIR ${LIBDIR})
endif()

set(FFMPEG_CFLAGS "\
-I${temp_LIBDIR}/lame/include \
-I${temp_LIBDIR}/openjpeg/include/ \
-I${temp_LIBDIR}/ogg/include \
-I${temp_LIBDIR}/vorbis/include \
-I${temp_LIBDIR}/theora/include \
-I${temp_LIBDIR}/opus/include \
-I${temp_LIBDIR}/vpx/include \
-I${temp_LIBDIR}/x264/include \
-I${temp_LIBDIR}/xvidcore/include \
-I${temp_LIBDIR}/zlib/include \
-I${temp_LIBDIR}/aom/include"
)
set(FFMPEG_LDFLAGS "\
-L${temp_LIBDIR}/lame/lib \
-L${temp_LIBDIR}/openjpeg/lib \
-L${temp_LIBDIR}/ogg/lib \
-L${temp_LIBDIR}/vorbis/lib \
-L${temp_LIBDIR}/theora/lib \
-L${temp_LIBDIR}/opus/lib \
-L${temp_LIBDIR}/vpx/lib \
-L${temp_LIBDIR}/x264/lib \
-L${temp_LIBDIR}/xvidcore/lib \
-L${temp_LIBDIR}/zlib/lib \
-L${temp_LIBDIR}/aom/lib"
)
set(FFMPEG_EXTRA_FLAGS
  --pkg-config-flags=--static
  --extra-cflags=${FFMPEG_CFLAGS}
  --extra-ldflags=${FFMPEG_LDFLAGS}
)
set(FFMPEG_ENV "PKG_CONFIG_PATH=\
${temp_LIBDIR}/openjpeg/lib/pkgconfig:\
${temp_LIBDIR}/x264/lib/pkgconfig:\
${temp_LIBDIR}/vorbis/lib/pkgconfig:\
${temp_LIBDIR}/ogg/lib/pkgconfig:\
${temp_LIBDIR}/vpx/lib/pkgconfig:\
${temp_LIBDIR}/theora/lib/pkgconfig:\
${temp_LIBDIR}/openjpeg/lib/pkgconfig:\
${temp_LIBDIR}/opus/lib/pkgconfig:\
${temp_LIBDIR}/aom/lib/pkgconfig:"
)

unset(temp_LIBDIR)

if(WIN32)
  set(FFMPEG_ENV set ${FFMPEG_ENV} &&)
  set(FFMPEG_EXTRA_FLAGS
    ${FFMPEG_EXTRA_FLAGS}
    --disable-static
    --enable-shared
    --enable-w32threads
    --disable-pthreads
    --enable-libopenjpeg
    --disable-mediafoundation
  )
else()
  set(FFMPEG_EXTRA_FLAGS
    ${FFMPEG_EXTRA_FLAGS}
    --enable-static
    --disable-shared
    --enable-libopenjpeg
  )
endif()

if(APPLE)
  set(FFMPEG_EXTRA_FLAGS
    ${FFMPEG_EXTRA_FLAGS}
    --target-os=darwin
    --x86asmexe=${LIBDIR}/nasm/bin/nasm
  )
elseif(UNIX)
  set(FFMPEG_EXTRA_FLAGS
    ${FFMPEG_EXTRA_FLAGS}
    --x86asmexe=${LIBDIR}/nasm/bin/nasm
  )
endif()

ExternalProject_Add(external_ffmpeg
  URL file://${PACKAGE_DIR}/${FFMPEG_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FFMPEG_HASH_TYPE}=${FFMPEG_HASH}
  # OpenJpeg is compiled with pthread support on Linux, which is all fine and is what we
  # want for maximum runtime performance, but due to static nature of that library we
  # need to force ffmpeg to link against pthread, otherwise test program used by autoconf
  # will fail. This patch does that in a way that is compatible with multiple distributions.
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/ffmpeg/src/external_ffmpeg < ${PATCH_DIR}/ffmpeg.diff
  PREFIX ${BUILD_DIR}/ffmpeg
  CONFIGURE_COMMAND ${CONFIGURE_ENV_NO_PERL} &&
    cd ${BUILD_DIR}/ffmpeg/src/external_ffmpeg/ &&
    ${FFMPEG_ENV} ${CONFIGURE_COMMAND_NO_TARGET} ${FFMPEG_EXTRA_FLAGS}
    --disable-lzma
    --disable-avfilter
    --disable-vdpau
    --disable-bzlib
    --disable-libgsm
    --disable-libspeex
    --enable-libvpx
    --enable-libopus
    --prefix=${LIBDIR}/ffmpeg
    --enable-libtheora
    --enable-libvorbis
    --enable-zlib
    --enable-stripping
    --enable-runtime-cpudetect
    --disable-vaapi
    --disable-nonfree
    --enable-gpl
    --disable-postproc
    --enable-libmp3lame
    --disable-librtmp
    --enable-libx264
    --enable-libxvid
    --enable-libaom
    --disable-libopencore-amrnb
    --disable-libopencore-amrwb
    --disable-libdc1394
    --disable-version3
    --disable-debug
    --enable-optimizations
    --enable-ffplay
    --disable-openssl
    --disable-securetransport
    --disable-indev=avfoundation
    --disable-indev=qtkit
    --disable-sdl2
    --disable-gnutls
    --disable-videotoolbox
    --disable-libxcb
    --disable-xlib
    --disable-audiotoolbox
    --disable-cuvid
    --disable-nvenc
    --disable-indev=jack
    --disable-indev=alsa
    --disable-outdev=alsa
    --disable-crystalhd
    --disable-sndio
  BUILD_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/ffmpeg/src/external_ffmpeg/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/ffmpeg/src/external_ffmpeg/ && make install
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ffmpeg ${DEFAULT_CMAKE_FLAGS}
  INSTALL_DIR ${LIBDIR}/ffmpeg
)

if(MSVC)
  set_target_properties(external_ffmpeg PROPERTIES FOLDER Mingw)
endif()

add_dependencies(
  external_ffmpeg
  external_zlib
  external_openjpeg
  external_xvidcore
  external_x264
  external_opus
  external_vpx
  external_theora
  external_vorbis
  external_ogg
  external_lame
  external_aom
)
if(WIN32)
  add_dependencies(
    external_ffmpeg
    external_zlib_mingw
  )
endif()
if(UNIX)
  add_dependencies(
    external_ffmpeg
    external_nasm
  )
endif()

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_ffmpeg after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/include ${HARVEST_TARGET}/ffmpeg/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/bin ${HARVEST_TARGET}/ffmpeg/lib
    DEPENDEES install
  )
endif()
