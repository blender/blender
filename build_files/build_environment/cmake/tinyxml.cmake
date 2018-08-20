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

set(TINYXML_EXTRA_ARGS
)

ExternalProject_Add(external_tinyxml
	URL ${TINYXML_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${TINYXML_HASH}
	PREFIX ${BUILD_DIR}/tinyxml
	#patch taken from ocio
	PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/tinyxml/src/external_tinyxml < ${PATCH_DIR}/tinyxml.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tinyxml ${DEFAULT_CMAKE_FLAGS} ${TINYXML_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/tinyxml
)

#if(BUILD_MODE STREQUAL Release AND WIN32)
	#ExternalProject_Add_Step(external_freetype after_install
	#	COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freetype ${HARVEST_TARGET}/freetype
	#	DEPENDEES install
	#)
#endif()
