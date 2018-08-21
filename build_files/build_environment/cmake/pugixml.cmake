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

set(PUGIXML_EXTRA_ARGS
)

ExternalProject_Add(external_pugixml
	URL ${PUGIXML_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${PUGIXML_HASH}
	PREFIX ${BUILD_DIR}/pugixml
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/pugixml ${DEFAULT_CMAKE_FLAGS} ${PUGIXML_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/pugixml
)
if(WIN32)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_pugixml after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/pugixml/lib/pugixml.lib ${HARVEST_TARGET}/osl/lib/pugixml.lib
			DEPENDEES install
		)
	endif()
	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_pugixml after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/pugixml/lib/pugixml.lib ${HARVEST_TARGET}/osl/lib/pugixml_d.lib
			DEPENDEES install
		)
	endif()	
endif()