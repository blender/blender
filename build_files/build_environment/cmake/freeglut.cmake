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

if(WIN32)
	if(BUILD_MODE STREQUAL Release)
		set(FREEGLUT_EXTRA_ARGS
			-DFREEGLUT_BUILD_SHARED_LIBS=Off
			-DFREEGLUT_BUILD_STATIC_LIBS=On
		)

		ExternalProject_Add(external_freeglut
			URL ${FREEGLUT_URI}
			DOWNLOAD_DIR ${DOWNLOAD_DIR}
			URL_HASH MD5=${FREEGLUT_HASH}
			PREFIX ${BUILD_DIR}/freeglut
			CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/freeglut ${DEFAULT_C_FLAGS} ${DEFAULT_CXX_FLAGS} ${FREEGLUT_EXTRA_ARGS}
			INSTALL_DIR ${LIBDIR}/freeglut
		)
	endif()
endif()
