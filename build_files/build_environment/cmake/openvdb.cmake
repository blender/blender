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
	-DBLOSC_blosc_LIBRARY=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
	-DOPENVDB_ENABLE_3_ABI_COMPATIBLE=OFF
	-DOPENVDB_BUILD_UNITTESTS=Off
	-DOPENVDB_BUILD_PYTHON_MODULE=Off
	-DGLEW_LOCATION=${LIBDIR}/glew/
	-DBLOSC_LOCATION=${LIBDIR}/blosc/
	-DTBB_LOCATION=${LIBDIR}/tbb/
	-DTBB_ROOT=${LIBDIR}/tbb/
	-DOPENEXR_LOCATION=${LIBDIR}/openexr
	-DILMBASE_LOCATION=${LIBDIR}/ilmbase
	-DIlmbase_HALF_LIBRARY=${LIBDIR}/ilmbase/lib/${LIBPREFIX}Half${ILMBASE_VERSION_POSTFIX}${LIBEXT}
	-DIlmbase_IEX_LIBRARY=${LIBDIR}/ilmbase/lib/${LIBPREFIX}Iex${ILMBASE_VERSION_POSTFIX}${LIBEXT}
	-DIlmbase_ILMTHREAD_LIBRARY=${LIBDIR}/ilmbase/lib/${LIBPREFIX}IlmThread${ILMBASE_VERSION_POSTFIX}${LIBEXT}
	-DOpenexr_ILMIMF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${LIBEXT}
	-DTBB_LIBRARYDIR=${LIBDIR}/tbb/lib
	-DTbb_TBB_LIBRARY=${LIBDIR}/tbb/lib/${LIBPREFIX}tbb_static${LIBEXT}
	-DTBB_LIBRARY_DIR=${LIBDIR}/tbb/lib
	-DTBB_LIBRARY_PATH=${LIBDIR}/tbb/lib
)

ExternalProject_Add(openvdb
	URL ${OPENVDB_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENVDB_HASH}
	PREFIX ${BUILD_DIR}/openvdb
	PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/openvdb/src/openvdb < ${PATCH_DIR}/openvdb.diff
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
