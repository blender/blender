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

set(LAME_EXTRA_ARGS)
if(MSVC)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
	set(LAME_EXTRA_ARGS CFLAGS=-msse)
	endif()
endif()

ExternalProject_Add(external_lame
	URL ${LAME_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${LAME_HASH}
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
