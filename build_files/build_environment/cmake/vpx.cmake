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
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(VPX_EXTRA_FLAGS --target=x86_64-win64-gcc --disable-multithread)
	else()
		set(VPX_EXTRA_FLAGS --target=x86-win32-gcc --disable-multithread)
	endif()
else()
	if(APPLE)
		set(VPX_EXTRA_FLAGS --target=x86_64-darwin13-gcc)
	else()
		set(VPX_EXTRA_FLAGS --target=generic-gnu)
	endif()
endif()

ExternalProject_Add(external_vpx
	URL ${VPX_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${VPX_HASH}
	PREFIX ${BUILD_DIR}/vpx
	CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
		cd ${BUILD_DIR}/vpx/src/external_vpx/ &&
		${CONFIGURE_COMMAND_NO_TARGET} --prefix=${LIBDIR}/vpx
			--disable-shared
			--enable-static
			--disable-install-bins
			--disable-install-srcs
			--disable-sse4_1
			--disable-sse3
			--disable-ssse3
			--disable-avx
			--disable-avx2
			--disable-unit-tests
			--disable-examples
			${VPX_EXTRA_FLAGS}
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vpx/src/external_vpx/ && make install
	INSTALL_DIR ${LIBDIR}/vpx
)

if(MSVC)
	set_target_properties(external_vpx PROPERTIES FOLDER Mingw)
endif()
