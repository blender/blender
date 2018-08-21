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
	set(SDL_EXTRA_ARGS
		-DSDL_STATIC=Off
	)
else()
	set(SDL_EXTRA_ARGS
		-DSDL_STATIC=ON
		-DSDL_SHARED=OFF
		-DSDL_VIDEO=OFF
	)
endif()

ExternalProject_Add(external_sdl
	URL ${SDL_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${SDL_HASH}
	PREFIX ${BUILD_DIR}/sdl
	PATCH_COMMAND ${PATCH_CMD} -p 0 -N -d ${BUILD_DIR}/sdl/src/external_sdl < ${PATCH_DIR}/sdl.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/sdl ${DEFAULT_CMAKE_FLAGS} ${SDL_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/sdl
)

if(BUILD_MODE STREQUAL Release AND WIN32)
	ExternalProject_Add_Step(external_sdl after_install
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/include/sdl2 ${HARVEST_TARGET}/sdl/include
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/lib ${HARVEST_TARGET}/sdl/lib
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/bin ${HARVEST_TARGET}/sdl/lib
		DEPENDEES install
	)
endif()
