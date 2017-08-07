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

ExternalProject_Add(external_zlib_mingw
	URL ${ZLIB_URI}
	URL_HASH MD5=${ZLIB_HASH}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	PREFIX ${BUILD_DIR}/zlib_mingw
	CONFIGURE_COMMAND echo .
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/zlib_mingw/src/external_zlib_mingw/ && make -f win32/makefile.gcc -j${MAKE_THREADS}
	INSTALL_COMMAND echo .
	INSTALL_DIR ${LIBDIR}/zlib_mingw
)

if(BUILD_MODE STREQUAL Release)
	ExternalProject_Add_Step(external_zlib_mingw after_install
		COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/zlib_mingw/src/external_zlib_mingw/libz.a ${LIBDIR}/zlib/lib/z.lib
		DEPENDEES install
	)
endif()

if(MSVC)
	set_target_properties(external_zlib_mingw PROPERTIES FOLDER Mingw)
endif()

