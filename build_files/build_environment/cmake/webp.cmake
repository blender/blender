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
	INSTALL_DIR ${LIBDIR}/webp
)
