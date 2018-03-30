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

set(SNDFILE_EXTRA_ARGS)
set(SNDFILE_ENV PKG_CONFIG_PATH=${mingw_LIBDIR}/ogg/lib/pkgconfig:${mingw_LIBDIR}/vorbis/lib/pkgconfig:${mingw_LIBDIR}/flac/lib/pkgconfig:${mingw_LIBDIR})

if(WIN32)
	set(SNDFILE_ENV set ${SNDFILE_ENV} &&)
	#shared for windows because static libs will drag in a libgcc dependency.
	set(SNDFILE_OPTIONS --disable-static --enable-shared )
else()
	set(SNDFILE_OPTIONS --enable-static --disable-shared )
endif()

if(UNIX)
	set(SNDFILE_PATCH_CMD ${PATCH_CMD} --verbose -p 0 -d ${BUILD_DIR}/sndfile/src/external_sndfile < ${PATCH_DIR}/sndfile.diff)
else()
	set(SNDFILE_PATCH_CMD)
endif()

ExternalProject_Add(external_sndfile
	URL ${SNDFILE_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${SNDFILE_HASH}
	PREFIX ${BUILD_DIR}/sndfile
	PATCH_COMMAND ${SNDFILE_PATCH_CMD}
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sndfile/src/external_sndfile/ && ${SNDFILE_ENV} ${CONFIGURE_COMMAND} ${SNDFILE_OPTIONS} --prefix=${mingw_LIBDIR}/sndfile
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sndfile/src/external_sndfile/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sndfile/src/external_sndfile/ && make install
	INSTALL_DIR ${LIBDIR}/sndfile
)

if(MSVC)
	set_target_properties(external_sndfile PROPERTIES FOLDER Mingw)
endif()

add_dependencies(
	external_sndfile
	external_ogg
	external_vorbis
)
if(UNIX)
	add_dependencies(
		external_sndfile
		external_flac
	)
endif()
