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

if(UNIX AND NOT APPLE)
	set(OPENCOLLADA_EXTRA_ARGS
		-DLIBXML2_INCLUDE_DIR=${LIBDIR}/xml2/include/libxml2
		-DLIBXML2_LIBRARIES=${LIBDIR}/xml2/lib/libxml2.a)
endif()

ExternalProject_Add(external_opencollada
	URL ${OPENCOLLADA_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENCOLLADA_HASH}
	PREFIX ${BUILD_DIR}/opencollada
	PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/opencollada/src/external_opencollada < ${PATCH_DIR}/opencollada.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencollada ${DEFAULT_CMAKE_FLAGS} ${OPENCOLLADA_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/opencollada
)

if(UNIX AND NOT APPLE)
	add_dependencies(
		external_opencollada
		external_xml2
	)
endif()
