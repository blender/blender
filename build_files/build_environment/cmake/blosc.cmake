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

set(BLOSC_EXTRA_ARGS
	-DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
	-DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
	-DBUILD_TESTS=OFF
	-DBUILD_BENCHMARKS=OFF
	-DCMAKE_DEBUG_POSTFIX=_d
	-DThreads_FOUND=1
	-DPTHREAD_LIBS=${LIBDIR}/pthreads/lib/pthreadVC2.lib
	-DPTHREAD_INCLUDE_DIR=${LIBDIR}/pthreads/inc
	-DDEACTIVATE_SNAPPY=ON
)

ExternalProject_Add(external_blosc
	URL ${BLOSC_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${BLOSC_HASH}
	PREFIX ${BUILD_DIR}/blosc
	#PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/blosc/src/external_blosc < ${PATCH_DIR}/blosc.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/blosc ${DEFAULT_CMAKE_FLAGS} ${BLOSC_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/blosc
)

add_dependencies(
	external_blosc
	external_zlib
)
if(WIN32)
	add_dependencies(
		external_blosc
		external_pthreads
	)
endif()

if (WIN32)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_blosc after_install
			COMMAND	${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc.lib ${HARVEST_TARGET}/blosc/lib/libblosc.lib
			COMMAND	${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/blosc/include/ ${HARVEST_TARGET}/blosc/include/
			DEPENDEES install
		)
	endif()
	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_blosc after_install
			COMMAND	${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc_d.lib ${HARVEST_TARGET}/blosc/lib/libblosc_d.lib
			DEPENDEES install
		)
	endif()
endif()

