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
	-DOCIO_BUILD_APPS=OFF
	-DOCIO_BUILD_PYGLUE=OFF
	-DOCIO_BUILD_NUKE=OFF
	-DOCIO_USE_BOOST_PTR=OFF
	-DOCIO_BUILD_STATIC=ON
	-DOCIO_BUILD_SHARED=OFF
	-DOCIO_BUILD_TRUELIGHT=OFF
	-DOCIO_BUILD_DOCS=OFF
	-DOCIO_BUILD_PYGLUE=OFF
	-DOCIO_BUILD_JNIGLUE=OFF
	-DOCIO_STATIC_JNIGLUE=OFF
)

if(WIN32)
	set(OCIO_PATCH opencolorio_win.diff)
	set(OPENCOLORIO_EXTRA_ARGS
		${OPENCOLORIO_EXTRA_ARGS}
		-DOCIO_BUILD_TESTS=OFF
		-DOCIO_USE_SSE=ON
		-DOCIO_INLINES_HIDDEN=OFF
		-DOCIO_PYGLUE_LINK=OFF
		-DOCIO_PYGLUE_RESPECT_ABI=OFF
		-DOCIO_PYGLUE_SONAME=OFF
		-DOCIO_PYGLUE_LIB_PREFIX=OFF
		-DUSE_EXTERNAL_TINYXML=ON
		-DTINYXML_INCLUDE_DIR=${LIBDIR}/tinyxml/include
		-DTINYXML_LIBRARY=${LIBDIR}/tinyxml/lib/tinyxml${libext}
		-DUSE_EXTERNAL_YAML=ON
		-DYAML_CPP_FOUND=ON
		-DYAML_CPP_VERSION=${YAMLCPP_VERSION}
		-DUSE_EXTERNAL_LCMS=ON
		-DINC_1=${LIBDIR}/tinyxml/include
		-DINC_2=${LIBDIR}/yamlcpp/include
		#lie because ocio cmake is demanding boost even though it is not needed
		-DYAML_CPP_VERSION=0.5.0
	)
else()
	set(OCIO_PATCH opencolorio.diff)
	set(OPENCOLORIO_EXTRA_ARGS
		${OPENCOLORIO_EXTRA_ARGS}
	)
endif()

ExternalProject_Add(external_opencolorio
	URL ${OPENCOLORIO_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENCOLORIO_HASH}
	PREFIX ${BUILD_DIR}/opencolorio
	PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/opencolorio/src/external_opencolorio < ${PATCH_DIR}/${OCIO_PATCH}
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

if(WIN32)
	add_dependencies(
		external_opencolorio
		external_tinyxml
		external_yamlcpp

	)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_opencolorio after_install
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/include ${HARVEST_TARGET}/opencolorio/include
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/lib/static ${HARVEST_TARGET}/opencolorio/lib
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/yamlcpp/lib/libyaml-cppmt.lib ${HARVEST_TARGET}/opencolorio/lib/libyaml-cpp.lib
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tinyxml/lib/tinyxml.lib ${HARVEST_TARGET}/opencolorio/lib/tinyxml.lib
			DEPENDEES install
		)
	endif()
	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_opencolorio after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opencolorio/lib/static/Opencolorio.lib ${HARVEST_TARGET}/opencolorio/lib/OpencolorIO_d.lib
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/yamlcpp/lib/libyaml-cppmtd.lib ${HARVEST_TARGET}/opencolorio/lib/libyaml-cpp_d.lib
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tinyxml/lib/tinyxml.lib ${HARVEST_TARGET}/opencolorio/lib/tinyxml_d.lib
			DEPENDEES install
		)
	endif()

endif()

