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

set(ICONV_EXTRA_ARGS)

ExternalProject_Add(external_iconv
	URL ${ICONV_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${ICONV_HASH}
	PREFIX ${BUILD_DIR}/iconv
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && ${CONFIGURE_COMMAND} --enable-static --prefix=${mingw_LIBDIR}/iconv
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && make install
	INSTALL_DIR ${LIBDIR}/iconv
)

if(MSVC)
	set_target_properties(external_iconv PROPERTIES FOLDER Mingw)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_iconv after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/lib/libiconv.a ${HARVEST_TARGET}/iconv/lib/iconv.lib
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/include/iconv.h ${HARVEST_TARGET}/iconv/include/iconv.h
			DEPENDEES install
		)
	endif()
endif()
