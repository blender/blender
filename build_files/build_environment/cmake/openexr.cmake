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
	set(OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
endif()

set(OPENEXR_EXTRA_ARGS
	-DBUILD_SHARED_LIBS=OFF
	-DCMAKE_CXX_STANDARD_LIBRARIES=${OPENEXR_CMAKE_CXX_STANDARD_LIBRARIES}
	-DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
	-DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
	-DILMBASE_PACKAGE_PREFIX=${LIBDIR}/ilmbase
)

ExternalProject_Add(external_openexr
	URL ${OPENEXR_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENEXR_HASH}
	PREFIX ${BUILD_DIR}/openexr
	PATCH_COMMAND ${PATCH_CMD} -p 0 -d ${BUILD_DIR}/openexr/src/external_openexr < ${PATCH_DIR}/openexr.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openexr ${DEFAULT_CMAKE_FLAGS} ${OPENEXR_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/openexr
)

add_dependencies(
	external_openexr
	external_zlib
	external_ilmbase
)
