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

set(EMBREE_EXTRA_ARGS
	-DEMBREE_ISPC_SUPPORT=OFF
	-DEMBREE_TUTORIALS=OFF
	-DEMBREE_STATIC_LIB=ON
	-DEMBREE_RAY_MASK=ON
	-DEMBREE_FILTER_FUNCTION=ON
	-DEMBREE_BACKFACE_CULLING=OFF
	-DEMBREE_TASKING_SYSTEM=INTERNAL
	-DEMBREE_MAX_ISA=AVX2
)

if(WIN32)
	set(EMBREE_BUILD_DIR ${BUILD_MODE}/)
else()
	set(EMBREE_BUILD_DIR)
endif()

ExternalProject_Add(external_embree
	URL ${EMBREE_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${EMBREE_HASH}
	PREFIX ${BUILD_DIR}/embree
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/embree ${DEFAULT_CMAKE_FLAGS} ${EMBREE_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/embree
)
