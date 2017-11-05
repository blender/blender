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

if(BUILD_MODE STREQUAL Release)
	if(WIN32)
		set(THUMB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../release/windows/blendthumb)

		ExternalProject_Add(external_zlib_32
			URL ${ZLIB_URI}
			CMAKE_GENERATOR ${GENERATOR_32}
			URL_HASH MD5=${ZLIB_HASH}
			DOWNLOAD_DIR ${DOWNLOAD_DIR}
			PREFIX ${BUILD_DIR}/zlib32
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zlib32 ${DEFAULT_CMAKE_FLAGS}
			INSTALL_DIR ${LIBDIR}/zlib32
		)

		ExternalProject_Add(external_zlib_64
			URL ${ZLIB_URI}
			CMAKE_GENERATOR ${GENERATOR_64}
			URL_HASH MD5=${ZLIB_HASH}
			DOWNLOAD_DIR ${DOWNLOAD_DIR}
			PREFIX ${BUILD_DIR}/zlib64
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zlib64 ${DEFAULT_CMAKE_FLAGS}
			INSTALL_DIR ${LIBDIR}/zlib64
		)

		ExternalProject_Add(external_blendthumb_32
			CMAKE_GENERATOR ${GENERATOR_32}
			SOURCE_DIR ${THUMB_DIR}
			PREFIX ${BUILD_DIR}/blendthumb32
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/blendThumb32 ${DEFAULT_CMAKE_FLAGS} -DZLIB_INCLUDE=${LIBDIR}/zlib32/include -DZLIB_LIBS=${LIBDIR}/zlib32/lib/zlibstatic.lib
			INSTALL_DIR ${LIBDIR}/blendthumb32
		)
		add_dependencies(
			external_blendthumb_32
			external_zlib_32
		)

		ExternalProject_Add(external_blendthumb_64
			CMAKE_GENERATOR ${GENERATOR_64}
			SOURCE_DIR ${THUMB_DIR}
			PREFIX ${BUILD_DIR}/blendthumb64
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/blendThumb64 ${DEFAULT_CMAKE_FLAGS} -DZLIB_INCLUDE=${LIBDIR}/zlib64/include -DZLIB_LIBS=${LIBDIR}/zlib64/lib/zlibstatic.lib
			INSTALL_DIR ${LIBDIR}/blendthumb64
		)
		add_dependencies(
			external_blendthumb_64
			external_zlib_64
		)
	endif()
endif()
