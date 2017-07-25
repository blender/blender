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

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(WEBP_EXTRA_ARGS
	-DWEBP_HAVE_SSE2=ON
	-DWEBP_HAVE_SSE41=OFF
	-DWEBP_HAVE_AVX2=OFF
)

if(WIN32)
	set(WEBP_BUILD_DIR ${BUILD_MODE}/)
else()
	set(WEBP_BUILD_DIR)
endif()

ExternalProject_Add(external_webp
	URL ${WEBP_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${WEBP_HASH}
	PREFIX ${BUILD_DIR}/webp
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/webp -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${WEBP_EXTRA_ARGS}
	INSTALL_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp-build/${WEBP_BUILD_DIR}${LIBPREFIX}webp${LIBEXT} ${LIBDIR}/webp/lib/${LIBPREFIX}webp${LIBEXT} &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/decode.h ${LIBDIR}/webp/include/webp/decode.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/encode.h ${LIBDIR}/webp/include/webp/encode.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/demux.h ${LIBDIR}/webp/include/webp/demux.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/extras.h ${LIBDIR}/webp/include/webp/extras.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/format_constants.h ${LIBDIR}/webp/include/webp/format_constants.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/mux.h ${LIBDIR}/webp/include/webp/mux.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/mux_types.h ${LIBDIR}/webp/include/webp/mux_types.h &&
		${CMAKE_COMMAND} -E copy ${BUILD_DIR}/webp/src/external_webp/src/webp/types.h ${LIBDIR}/webp/include/webp/types.h
	INSTALL_DIR ${LIBDIR}/webp
)
