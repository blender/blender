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

set(HIDAPI_EXTRA_ARGS)

ExternalProject_Add(external_hidapi
	URL ${HIDAPI_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${HIDAPI_HASH}
	PREFIX ${BUILD_DIR}/hidapi
	PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_hidapi.txt	${BUILD_DIR}/hidapi/src/external_hidapi/cmakelists.txt && ${PATCH_CMD} -p 0 -d ${BUILD_DIR}/hidapi/src/external_hidapi < ${PATCH_DIR}/hidapi.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/hidapi -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${HIDAPI_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/hidapi
)
