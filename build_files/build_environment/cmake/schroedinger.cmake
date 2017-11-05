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
	set(SCHROEDINGER_EXTRA_FLAGS "CFLAGS=-g -I./ -I${LIBDIR}/orc/include/orc-0.4" "LDFLAGS=-Wl,--as-needed -static-libgcc -L${LIBDIR}/orc/lib" ORC_CFLAGS=-I${LIBDIR}/orc/include/orc-0.4 ORC_LDFLAGS=-L${LIBDIR}/orc/lib ORC_LIBS=${LIBDIR}/orc/lib/liborc-0.4.a ORCC=${LIBDIR}/orc/bin/orcc.exe)
else()
	set(SCHROEDINGER_CFLAGS "${PLATFORM_CFLAGS} -I./ -I${LIBDIR}/orc/include/orc-0.4")
	set(SCHROEDINGER_LDFLAGS "${PLATFORM_LDFLAGS} -L${LIBDIR}/orc/lib")
	set(SCHROEDINGER_EXTRA_FLAGS CFLAGS=${SCHROEDINGER_CFLAGS} LDFLAGS=${SCHROEDINGER_LDFLAGS} ORC_CFLAGS=-I${LIBDIR}/orc/include/orc-0.4 ORC_LDFLAGS=-L${LIBDIR}/orc/lib ORCC=${LIBDIR}/orc/bin/orcc) # ORC_LIBS=${LIBDIR}/orc/lib/liborc-0.4.a
endif()

ExternalProject_Add(external_schroedinger
	URL ${SCHROEDINGER_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH SHA256=${SCHROEDINGER_HASH}
	PREFIX ${BUILD_DIR}/schroedinger
	PATCH_COMMAND ${PATCH_CMD} --verbose -p 0 -N -d ${BUILD_DIR}/schroedinger/src/external_schroedinger < ${PATCH_DIR}/schroedinger.diff
	CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
		cd ${BUILD_DIR}/schroedinger/src/external_schroedinger/ &&
		${CONFIGURE_COMMAND} --prefix=${LIBDIR}/schroedinger --disable-shared --enable-static ${SCHROEDINGER_EXTRA_FLAGS}
	BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/schroedinger/src/external_schroedinger/ && make -j${MAKE_THREADS}
	INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/schroedinger/src/external_schroedinger/ && make install
	INSTALL_DIR ${LIBDIR}/schroedinger
)

add_dependencies(
	external_schroedinger
	external_orc
)

if(MSVC)
	set_target_properties(external_schroedinger PROPERTIES FOLDER Mingw)
endif()
