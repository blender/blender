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

if(BUILD_MODE STREQUAL Debug)
	set(BLOSC_POST _d)
endif()

set(OPENVDB_EXTRA_ARGS
	-DILMBASE_HOME=${LIBDIR}/ilmbase/
	-DILMBASE_CUSTOM=ON
	-DILMBASE_CUSTOM_LIBRARIES=Half;Imath-2_2;IlmThread-2_2;Iex-2_2
	-DILMBASE_INCLUDE_DIR=${LIBDIR}/ilmbase/include/
	-DILMBASE_HALF_LIBRARIES=${LIBDIR}/ilmbase/lib/Half${LIBEXT}
	-DILMBASE_IMATH_LIBRARIES=${LIBDIR}/ilmbase/lib/${LIBPREFIX}Imath-2_2${LIBEXT}
	-DILMBASE_ILMTHREAD_LIBRARIES=${LIBDIR}/ilmbase/lib/${LIBPREFIX}IlmThread-2_2${LIBEXT}
	-DILMBASE_IEX_LIBRARIES=${LIBDIR}/ilmbase/lib/${LIBPREFIX}Iex-2_2${LIBEXT}
	-DOPENEXR_HOME=${LIBDIR}/openexr/
	-DOPENEXR_USE_STATIC_LIBS=ON
	-DOPENEXR_CUSTOM=ON
	-DOPENEXR_CUSTOM_LIBRARY=IlmImf-2_2
	-DOPENEXR_INCLUDE_DIR=${LIBDIR}/openexr/include/
	-DOPENEXR_ILMIMF_LIBRARIES=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf-2_2${LIBEXT}
	-DTBB_ROOT_DIR=${LIBDIR}/tbb/
	-DTBB_INCLUDE_DIRS=${LIBDIR}/tbb/include
	-DTBB_LIBRARY=${LIBDIR}/tbb/lib/tbb_static${LIBEXT}
	-DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
	-DBoost_USE_MULTITHREADED=ON
	-DBoost_USE_STATIC_LIBS=ON
	-DBoost_USE_STATIC_RUNTIME=ON
	-DBOOST_ROOT=${LIBDIR}/boost
	-DBoost_NO_SYSTEM_PATHS=ON
	-DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
	-DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
	-DWITH_BLOSC=ON
	-DBLOSC_INCLUDE_DIR=${LIBDIR}/blosc/include/
	-DBLOSC_LIBRARY=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
)

set(OPENVDB_EXTRA_ARGS ${OPENVDB_EXTRA_ARGS})

# CMake script for OpenVDB based on https://raw.githubusercontent.com/diekev/openvdb-cmake/master/CMakeLists.txt
# can't be in external_openvdb because of how the includes are setup.

ExternalProject_Add(openvdb
	URL ${OPENVDB_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENVDB_HASH}
	PREFIX ${BUILD_DIR}/openvdb
	PATCH_COMMAND COMMAND
		${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_openvdb.txt  ${BUILD_DIR}/openvdb/src/openvdb/CMakeLists.txt &&
		${CMAKE_COMMAND} -E copy_directory ${PATCH_DIR}/cmake/  ${BUILD_DIR}/openvdb/src/openvdb/cmake/ &&
		${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/openvdb/src/openvdb < ${PATCH_DIR}/openvdb_vc2013.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openvdb ${DEFAULT_CMAKE_FLAGS} ${OPENVDB_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/openvdb
)

add_dependencies(
	openvdb
	external_tbb
	external_boost
	external_ilmbase
	external_openexr
	external_zlib
	external_blosc
)
