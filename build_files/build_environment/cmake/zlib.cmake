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

ExternalProject_Add(external_zlib
	URL ${ZLIB_URI}
	URL_HASH MD5=${ZLIB_HASH}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	PREFIX ${BUILD_DIR}/zlib
	CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zlib ${DEFAULT_CMAKE_FLAGS}
	INSTALL_DIR ${LIBDIR}/zlib
)

if (WIN32)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_zlib after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstatic${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st${LIBEXT}
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zlib/include/ ${HARVEST_TARGET}/zlib/include/
			DEPENDEES install
		)
	endif()
	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_zlib after_install
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstaticd${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st_d${LIBEXT}
		DEPENDEES install
		)
	endif()
else()
	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_zlib after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstaticd${LIBEXT} ${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
			DEPENDEES install
		)
	endif()
endif()
