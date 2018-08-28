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

set(BZIP2_PREFIX "${LIBDIR}/bzip2")
set(BZIP2_CONFIGURE_ENV echo .)
set(BZIP2_CONFIGURATION_ARGS)

if(UNIX AND NOT APPLE)
	set(BZIP2_LDFLAGS "-Wl,--as-needed")
	set(BZIP2_CFLAGS "-fPIC -Wall -Winline -O2 -g -D_FILE_OFFSET_BITS=64")
	set(BZIP2_CONFIGURE_ENV ${BZIP2_CONFIGURE_ENV} && export LDFLAGS=${BZIP2_LDFLAGS} && export CFLAGS=${BZIP2_CFLAGS}
		&& export PREFIX=${BZIP2_PREFIX})
endif()

ExternalProject_Add(external_bzip2
	URL ${BZIP2_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${BZIP2_HASH}
	PREFIX ${BUILD_DIR}/bzip2
	CONFIGURE_COMMAND echo .
	BUILD_COMMAND ${BZIP2_CONFIGURE_ENV} && cd ${BUILD_DIR}/bzip2/src/external_bzip2/ && make CFLAGS=${BZIP2_CFLAGS} LDFLAGS=${BZIP2_LDFLAGS} -j${MAKE_THREADS}
	INSTALL_COMMAND ${BZIP2_CONFIGURE_ENV} && cd ${BUILD_DIR}/bzip2/src/external_bzip2/ && make CFLAGS=${BZIP2_CFLAGS} LDFLAGS=${BZIP2_LDFLAGS} PREFIX=${BZIP2_PREFIX} install
	INSTALL_DIR ${LIBDIR}/bzip2
)
