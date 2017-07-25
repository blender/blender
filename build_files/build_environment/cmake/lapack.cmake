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

set(LAPACK_EXTRA_ARGS)

if(WIN32)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(LAPACK_EXTRA_ARGS -G "MSYS Makefiles" -DCMAKE_Fortran_COMPILER=${DOWNLOAD_DIR}/mingw/mingw64/bin/gfortran.exe)
	else()
		set(LAPACK_EXTRA_ARGS -G "MSYS Makefiles" -DCMAKE_Fortran_COMPILER=${DOWNLOAD_DIR}/mingw/mingw32/bin/gfortran.exe)
	endif()
endif()

ExternalProject_Add(external_lapack
	URL ${LAPACK_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${LAPACK_HASH}
	PREFIX ${BUILD_DIR}/lapack
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lapack/src/external_lapack/ && ${CMAKE_COMMAND} ${LAPACK_EXTRA_ARGS} -DBUILD_TESTING=Off -DCMAKE_INSTALL_PREFIX=${LIBDIR}/lapack .
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lapack/src/external_lapack/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/lapack/src/external_lapack/ && make install

	INSTALL_DIR ${LIBDIR}/lapack
)

if(MSVC)
	set_target_properties(external_lapack PROPERTIES FOLDER Mingw)
endif()
