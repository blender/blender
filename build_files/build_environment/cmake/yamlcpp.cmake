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

set(YAMLCPP_EXTRA_ARGS
	-DBUILD_GMOCK=OFF
	-DYAML_CPP_BUILD_TESTS=OFF
	-DYAML_CPP_BUILD_TOOLS=OFF
	-DYAML_CPP_BUILD_CONTRIB=OFF
	-DMSVC_SHARED_RT=OFF
)

ExternalProject_Add(external_yamlcpp
	URL ${YAMLCPP_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${YAMLCPP_HASH}
	PREFIX ${BUILD_DIR}/yamlcpp
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/yamlcpp ${DEFAULT_CMAKE_FLAGS} ${YAMLCPP_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/yamlcpp
)

#if(BUILD_MODE STREQUAL Release AND WIN32)
	#ExternalProject_Add_Step(external_freetype after_install
	#	COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freetype ${HARVEST_TARGET}/freetype
	#	DEPENDEES install
	#)
#endif()
