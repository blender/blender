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

# Note the encoder/decoder may use png/tiff/lcms system libraries, but the
# library itself does not depend on them, so should give no problems.

set(OPENJPEG_EXTRA_ARGS -DBUILD_SHARED_LIBS=OFF)

if(WIN32)
	set(OPENJPEG_EXTRA_ARGS -G "MSYS Makefiles" -DBUILD_PKGCONFIG_FILES=On)
else()
	set(OPENJPEG_EXTRA_ARGS ${DEFAULT_CMAKE_FLAGS})
endif()

ExternalProject_Add(external_openjpeg
	URL ${OPENJPEG_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${OPENJPEG_HASH}
	PREFIX ${BUILD_DIR}/openjpeg
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build && ${CMAKE_COMMAND} ${OPENJPEG_EXTRA_ARGS} -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openjpeg -DBUILD_SHARED_LIBS=Off -DBUILD_THIRDPARTY=OFF ${BUILD_DIR}/openjpeg/src/external_openjpeg
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/openjpeg/src/external_openjpeg-build/ && make install
	INSTALL_DIR ${LIBDIR}/openjpeg
)

#on windows ffmpeg wants a mingw build, while oiio needs a msvc build
if(MSVC)
	set(OPENJPEG_EXTRA_ARGS ${DEFAULT_CMAKE_FLAGS})
	ExternalProject_Add(external_openjpeg_msvc
		URL ${OPENJPEG_URI}
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL_HASH SHA256=${OPENJPEG_HASH}
		PREFIX ${BUILD_DIR}/openjpeg_msvc
		CMAKE_ARGS ${OPENJPEG_EXTRA_ARGS} -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openjpeg_msvc -DBUILD_SHARED_LIBS=Off -DBUILD_THIRDPARTY=OFF
		INSTALL_DIR ${LIBDIR}/openjpeg_msvc
	)
	if(BUILD_MODE STREQUAL Release)
		ExternalProject_Add_Step(external_openjpeg_msvc after_install
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openjpeg_msvc/lib ${HARVEST_TARGET}/openjpeg/lib &&
					${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openjpeg_msvc/include ${HARVEST_TARGET}/openjpeg/include
			DEPENDEES install
		)
	endif()
endif()

set(OPENJPEG_LIBRARY libopenjp2${LIBEXT})
if(MSVC)
	set_target_properties(external_openjpeg PROPERTIES FOLDER Mingw)
endif()
