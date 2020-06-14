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
  -DEMBREE_MAX_ISA=AVX2
  -DEMBREE_TASKING_SYSTEM=TBB
  -DEMBREE_TBB_ROOT=${LIBDIR}/tbb
  -DTBB_STATIC_LIB=${TBB_STATIC_LIBRARY}
)

if(TBB_STATIC_LIBRARY)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DEMBREE_TBB_LIBRARY_NAME=tbb_static
    -DEMBREE_TBBMALLOC_LIBRARY_NAME=tbbmalloc_static
  )
endif()

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
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/embree/src/external_embree < ${PATCH_DIR}/embree.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/embree ${DEFAULT_CMAKE_FLAGS} ${EMBREE_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/embree
)

add_dependencies(
  external_embree
  external_tbb
)

if(WIN32)

  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_embree after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/embree ${HARVEST_TARGET}/embree
      DEPENDEES install
    )
  else()
  ExternalProject_Add_Step(external_embree after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/embree3.lib ${HARVEST_TARGET}/embree/lib/embree3_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/embree_avx.lib ${HARVEST_TARGET}/embree/lib/embree_avx_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/embree_avx2.lib ${HARVEST_TARGET}/embree/lib/embree_avx2_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/embree_sse42.lib ${HARVEST_TARGET}/embree/lib/embree_sse42_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/lexers.lib ${HARVEST_TARGET}/embree/lib/lexers_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/math.lib ${HARVEST_TARGET}/embree/lib/math_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/simd.lib ${HARVEST_TARGET}/embree/lib/simd_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/sys.lib ${HARVEST_TARGET}/embree/lib/sys_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/tasking.lib ${HARVEST_TARGET}/embree/lib/tasking_d.lib
      DEPENDEES install
    )
  endif()

endif()
