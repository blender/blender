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

set(GLEW_EXTRA_ARGS
	-DBUILD_UTILS=Off
	-DBUILD_SHARED_LIBS=Off
)

ExternalProject_Add(external_glew
	URL ${GLEW_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${GLEW_HASH}
	PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_glew.txt	${BUILD_DIR}/glew/src/external_glew/CMakeLists.txt
	PREFIX ${BUILD_DIR}/glew
	CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/glew ${DEFAULT_CMAKE_FLAGS} ${GLEW_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/glew
)
