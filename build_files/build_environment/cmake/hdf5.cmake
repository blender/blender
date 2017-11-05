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

set(HDF5_EXTRA_ARGS
	-DHDF5_ENABLE_THREADSAFE=Off
	-DHDF5_BUILD_CPP_LIB=Off
	-DBUILD_TESTING=Off
	-DHDF5_BUILD_TOOLS=Off
	-DHDF5_BUILD_EXAMPLES=Off
	-DHDF5_BUILD_HL_LIB=On
	-DBUILD_STATIC_CRT_LIBS=On
	-DBUILD_SHARED_LIBS=On
)

if(WIN32)
	set(HDF5_PATCH ${PATCH_CMD} --verbose -p 0 -d ${BUILD_DIR}/hdf5/src/external_hdf5 < ${PATCH_DIR}/hdf5.diff)
endif()

ExternalProject_Add(external_hdf5
	URL ${HDF5_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${HDF5_HASH}
	PREFIX ${BUILD_DIR}/hdf5
	PATCH_COMMAND ${HDF5_PATCH}
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/hdf5 ${HDF5_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/hdf5
)
