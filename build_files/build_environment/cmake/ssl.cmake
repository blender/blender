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

set(SSL_CONFIGURE_COMMAND ./Configure)
set(SSL_PATCH_CMD echo .)

if (APPLE)
	set(SSL_OS_COMPILER "blender-darwin-x86_64")
else()
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(SSL_EXTRA_ARGS enable-ec_nistp_64_gcc_128)
		set(SSL_OS_COMPILER "blender-linux-x86_64")
	else()
		set(SSL_OS_COMPILER "blender-linux-x86")
	endif()
endif()

ExternalProject_Add(external_ssl
	URL ${SSL_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${SSL_HASH}
	PREFIX ${BUILD_DIR}/ssl
	PATCH_COMMAND ${SSL_PATCH_CMD}
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ssl/src/external_ssl/ && ${SSL_CONFIGURE_COMMAND} --prefix=${LIBDIR}/ssl
		--openssldir=${LIBDIR}/ssl
		no-shared
		no-idea no-mdc2 no-rc5 no-zlib no-ssl3 enable-unit-test no-ssl3-method enable-rfc3779 enable-cms
		--config=${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl.conf
		${SSL_OS_COMPILER}
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ssl/src/external_ssl/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/ssl/src/external_ssl/ && make install
	INSTALL_DIR ${LIBDIR}/ssl
)
