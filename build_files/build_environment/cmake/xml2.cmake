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

ExternalProject_Add(external_xml2
	URL ${XML2_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${XML2_HASH}
	PREFIX ${BUILD_DIR}/xml2
	CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && ${CONFIGURE_COMMAND}
		--prefix=${LIBDIR}/xml2
		--disable-shared
		--enable-static
		--with-pic
		--with-python=no
		--with-lzma=no
		--with-zlib=no
		--with-iconv=no
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xml2/src/external_xml2/ && make install
	INSTALL_DIR ${LIBDIR}/xml2
)
