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

set(CUEW_EXTRA_ARGS)

ExternalProject_Add(external_cuew
	URL ${CUEW_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${CUEW_HASH}
	PREFIX ${BUILD_DIR}/cuew
	PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/cuew/src/external_cuew < ${PATCH_DIR}/cuew.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/cuew -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${CUEW_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/cuew
)
