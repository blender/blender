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
	# cmake for windows
	set(JPEG_EXTRA_ARGS -DNASM=${NASM_PATH} -DWITH_JPEG8=ON  -DCMAKE_DEBUG_POSTFIX=d)

	ExternalProject_Add(external_jpeg
		URL ${JPEG_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH MD5=${JPEG_HASH}
		PREFIX ${BUILD_DIR}/jpg
		CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/jpg ${DEFAULT_CMAKE_FLAGS} ${JPEG_EXTRA_ARGS}
		INSTALL_DIR ${LIBDIR}/jpg
	)

	if(BUILD_MODE STREQUAL Debug)
		ExternalProject_Add_Step(external_jpeg after_install
			COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/jpg/lib/jpegd${LIBEXT}  ${LIBDIR}/jpg/lib/jpeg${LIBEXT}
			DEPENDEES install
		)
	endif()

	if(BUILD_MODE STREQUAL Release)
		set(JPEG_LIBRARY jpeg-static${LIBEXT})
	else()
		set(JPEG_LIBRARY jpeg-staticd${LIBEXT})
	endif()
else(WIN32)
	# autoconf for unix
	if(APPLE)
		set(JPEG_EXTRA_ARGS --host x86_64-apple-darwin --with-jpeg8)
	else()
		set(JPEG_EXTRA_ARGS --with-jpeg8)
	endif()

	ExternalProject_Add(external_jpeg
		URL ${JPEG_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH MD5=${JPEG_HASH}
		CONFIGURE_COMMAND ${CONFIGURE_ENV} && autoreconf -fiv && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/jpg NASM=yasm ${JPEG_EXTRA_ARGS}
		BUILD_IN_SOURCE 1
		BUILD_COMMAND ${CONFIGURE_ENV} && make install
		PREFIX ${BUILD_DIR}/jpg
		CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/jpg ${DEFAULT_CMAKE_FLAGS} ${JPEG_EXTRA_ARGS}
		INSTALL_DIR ${LIBDIR}/jpg
	)

	set(JPEG_LIBRARY libjpeg${LIBEXT})
endif(WIN32)
