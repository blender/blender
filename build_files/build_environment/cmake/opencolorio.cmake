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

set(OPENCOLORIO_EXTRA_ARGS
	-DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
	-DBoost_USE_MULTITHREADED=ON
	-DBoost_USE_STATIC_LIBS=ON
	-DBoost_USE_STATIC_RUNTIME=ON
	-DBOOST_ROOT=${LIBDIR}/boost
	-DBOOST_INCLUDEDIR=${LIBDIR}/boost/include/boost_1_60/boost
	-DBoost_NO_SYSTEM_PATHS=ON
	-DBoost_DEBUG=ON
	-DBoost_MAJOR_VERSION=1
	-DBoost_MINOR_VERSION=60
	-DOCIO_BUILD_APPS=OFF
	-DOCIO_BUILD_PYGLUE=OFF
	-DOCIO_BUILD_NUKE=OFF
)

if(WIN32)
	set(OPENCOLORIO_EXTRA_ARGS
		${OPENCOLORIO_EXTRA_ARGS}
		-DOCIO_USE_BOOST_PTR=ON
		-DOCIO_BUILD_STATIC=OFF
		-DOCIO_BUILD_SHARED=ON
	)
else()
	set(OPENCOLORIO_EXTRA_ARGS
		${OPENCOLORIO_EXTRA_ARGS}
		-DOCIO_USE_BOOST_PTR=OFF
		-DOCIO_BUILD_STATIC=ON
		-DOCIO_BUILD_SHARED=OFF
	)
endif()

ExternalProject_Add(external_opencolorio
	URL ${OPENCOLORIO_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENCOLORIO_HASH}
	PREFIX ${BUILD_DIR}/opencolorio
	PATCH_COMMAND ${PATCH_CMD} -p 0 -N -d ${BUILD_DIR}/opencolorio/src/external_opencolorio < ${PATCH_DIR}/opencolorio.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencolorio ${DEFAULT_CMAKE_FLAGS} ${OPENCOLORIO_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/opencolorio
)

if(NOT WIN32)
	add_custom_command(
		OUTPUT ${LIBDIR}/opencolorio/lib/libtinyxml.a
		COMMAND cp ${BUILD_DIR}/opencolorio/src/external_opencolorio-build/ext/dist/lib/libtinyxml.a ${LIBDIR}/opencolorio/lib/libtinyxml.a
		COMMAND cp ${BUILD_DIR}/opencolorio/src/external_opencolorio-build/ext/dist/lib/libyaml-cpp.a ${LIBDIR}/opencolorio/lib/libyaml-cpp.a
	)
	add_custom_target(external_opencolorio_extra ALL DEPENDS external_opencolorio ${LIBDIR}/opencolorio/lib/libtinyxml.a)
endif()

add_dependencies(
	external_opencolorio
	external_boost
)
