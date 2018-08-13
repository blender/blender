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

if(BUILD_MODE STREQUAL Release)
	set(OPENAL_EXTRA_ARGS
		-DALSOFT_UTILS=Off
		-DALSOFT_NO_CONFIG_UTIL=On
		-DALSOFT_EXAMPLES=Off
		-DALSOFT_TESTS=Off
		-DALSOFT_CONFIG=Off
		-DALSOFT_HRTF_DEFS=Off
		-DALSOFT_INSTALL=On
		-DALSOFT_BACKEND_SNDIO=Off
	)

	if(UNIX)
		set(OPENAL_EXTRA_ARGS ${OPENAL_EXTRA_ARGS} -DLIBTYPE=STATIC)
	endif()

	ExternalProject_Add(external_openal
		URL ${OPENAL_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH MD5=${OPENAL_HASH}
		PREFIX ${BUILD_DIR}/openal
		CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openal ${DEFAULT_CMAKE_FLAGS} ${OPENAL_EXTRA_ARGS}
		INSTALL_DIR ${LIBDIR}/openal
		PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/openal/src/external_openal < ${PATCH_DIR}/openal.diff
	)

	if(WIN32)
		ExternalProject_Add_Step(external_openal after_install
			COMMAND	${CMAKE_COMMAND} -E copy ${LIBDIR}/openal/lib/openal32.lib ${HARVEST_TARGET}/openal/lib/openal32.lib
			COMMAND	${CMAKE_COMMAND} -E copy ${LIBDIR}/openal/bin/openal32.dll ${HARVEST_TARGET}/openal/lib/openal32.dll
			COMMAND	${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openal/include/ ${HARVEST_TARGET}/openal/include/
			DEPENDEES install
		)
	endif()

endif()
