# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

set(FFMPEG_CFLAGS "-I${mingw_LIBDIR}/lame/include -I${mingw_LIBDIR}/openjpeg/include/ -I${mingw_LIBDIR}/ogg/include -I${mingw_LIBDIR}/vorbis/include -I${mingw_LIBDIR}/theora/include -I${mingw_LIBDIR}/vpx/include -I${mingw_LIBDIR}/x264/include -I${mingw_LIBDIR}/xvidcore/include -I${mingw_LIBDIR}/zlib/include")
set(FFMPEG_LDFLAGS "-L${mingw_LIBDIR}/lame/lib -L${mingw_LIBDIR}/openjpeg/lib -L${mingw_LIBDIR}/ogg/lib -L${mingw_LIBDIR}/vorbis/lib -L${mingw_LIBDIR}/theora/lib -L${mingw_LIBDIR}/vpx/lib -L${mingw_LIBDIR}/x264/lib -L${mingw_LIBDIR}/xvidcore/lib -L${mingw_LIBDIR}/zlib/lib")
if(UNIX AND NOT APPLE)
	# OpenJpeg is compiled with pthread support on Linux, which is all fine and is what we
	# want for maximum runtime performance, but due to static nature of that library we
	# need to force FFpeg to link against pthread, otherwise test program used by autoconf
	# will fail.
	set(FFMPEG_LDFLAGS "${FFMPEG_LDFLAGS} -lpthread")
endif()
set(FFMPEG_EXTRA_FLAGS --pkg-config-flags=--static --extra-cflags=${FFMPEG_CFLAGS} --extra-ldflags=${FFMPEG_LDFLAGS})
set(FFMPEG_ENV PKG_CONFIG_PATH=${mingw_LIBDIR}/openjpeg/lib/pkgconfig:${mingw_LIBDIR}/x264/lib/pkgconfig:${mingw_LIBDIR}/vorbis/lib/pkgconfig:${mingw_LIBDIR}/ogg/lib/pkgconfig:${mingw_LIBDIR})

if(WIN32)
	set(FFMPEG_ENV set ${FFMPEG_ENV} &&)
	set(FFMPEG_EXTRA_FLAGS
		${FFMPEG_EXTRA_FLAGS}
		--disable-static
		--enable-shared
		--enable-w32threads
		--disable-pthreads
		--enable-libopenjpeg
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
		)
endif()

ExternalProject_Add(external_ffmpeg
	URL ${FFMPEG_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${FFMPEG_HASH}
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
		--disable-libopencore-amrnb
		--disable-libopencore-amrwb
		--disable-libdc1394
		--disable-version3
		--disable-debug
		--enable-optimizations
		--disable-sse
		--disable-ssse3
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
	external_faad
	external_openjpeg
	external_xvidcore
	external_x264
	external_vpx
	external_theora
	external_vorbis
	external_ogg
	external_lame
)
if(WIN32)
	add_dependencies(
		external_ffmpeg
		external_zlib_mingw
	)
endif()

if(BUILD_MODE STREQUAL Release AND WIN32)
	ExternalProject_Add_Step(external_ffmpeg after_install
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/include ${HARVEST_TARGET}/ffmpeg/include
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/bin ${HARVEST_TARGET}/ffmpeg/lib
		DEPENDEES install
	)
endif()
