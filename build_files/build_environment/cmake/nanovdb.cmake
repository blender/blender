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

set(NANOVDB_EXTRA_ARGS
   # NanoVDB is header-only, so only need the install target
  -DNANOVDB_BUILD_UNITTESTS=OFF
  -DNANOVDB_BUILD_EXAMPLES=OFF
  -DNANOVDB_BUILD_BENCHMARK=OFF
  -DNANOVDB_BUILD_DOCS=OFF
  -DNANOVDB_BUILD_TOOLS=OFF
  -DNANOVDB_CUDA_KEEP_PTX=OFF
   # Do not need to include any of the dependencies because of this
  -DNANOVDB_USE_OPENVDB=OFF
  -DNANOVDB_USE_OPENGL=OFF
  -DNANOVDB_USE_OPENCL=OFF
  -DNANOVDB_USE_CUDA=OFF
  -DNANOVDB_USE_TBB=OFF
  -DNANOVDB_USE_BLOSC=OFF
  -DNANOVDB_USE_ZLIB=OFF
  -DNANOVDB_USE_OPTIX=OFF
  -DNANOVDB_ALLOW_FETCHCONTENT=OFF
)

ExternalProject_Add(nanovdb
  URL ${NANOVDB_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${NANOVDB_HASH}
  PREFIX ${BUILD_DIR}/nanovdb
  SOURCE_SUBDIR nanovdb
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/nanovdb ${DEFAULT_CMAKE_FLAGS} ${NANOVDB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/nanovdb
)

if(WIN32)
  ExternalProject_Add_Step(nanovdb after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/nanovdb/nanovdb ${HARVEST_TARGET}/nanovdb/include/nanovdb
    DEPENDEES install
  )
endif()
