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

set(LCMS_EXTRA_ARGS
)

ExternalProject_Add(external_lcms
	URL ${LCMS_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${LCMS_HASH}
	PREFIX ${BUILD_DIR}/lcms
	#patch taken from ocio
	PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_lcms.txt ${BUILD_DIR}/lcms/src/external_lcms/CMakeLists.txt
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/lcms ${DEFAULT_CMAKE_FLAGS} ${LCMS_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/lcms
)

#if(BUILD_MODE STREQUAL Release AND WIN32)
	#ExternalProject_Add_Step(external_freetype after_install
	#	COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freetype ${HARVEST_TARGET}/freetype
	#	DEPENDEES install
	#)
#endif()
