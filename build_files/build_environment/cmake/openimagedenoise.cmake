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


set(OIDN_EXTRA_ARGS
  -DWITH_EXAMPLE=OFF
  -DWITH_TEST=OFF
  -DTBB_ROOT=${LIBDIR}/tbb
  -DTBB_STATIC_LIB=${TBB_STATIC_LIBRARY}
  -DOIDN_STATIC_LIB=ON
)

ExternalProject_Add(external_openimagedenoise
  URL ${OIDN_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${OIDN_HASH}
  PREFIX ${BUILD_DIR}/openimagedenoise
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openimagedenoise ${DEFAULT_CMAKE_FLAGS} ${OIDN_EXTRA_ARGS}
  PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/openimagedenoise/src/external_openimagedenoise < ${PATCH_DIR}/openimagedenoise.diff
  INSTALL_DIR ${LIBDIR}/openimagedenoise
)

add_dependencies(
  external_openimagedenoise
  external_tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openimagedenoise after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openimagedenoise/include ${HARVEST_TARGET}/openimagedenoise/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/openimagedenoise.lib ${HARVEST_TARGET}/openimagedenoise/lib/openimagedenoise.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/common.lib ${HARVEST_TARGET}/openimagedenoise/lib/common.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/mkldnn.lib ${HARVEST_TARGET}/openimagedenoise/lib/mkldnn.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_openimagedenoise after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/openimagedenoise.lib ${HARVEST_TARGET}/openimagedenoise/lib/openimagedenoise_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/common.lib ${HARVEST_TARGET}/openimagedenoise/lib/common_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimagedenoise/lib/mkldnn.lib ${HARVEST_TARGET}/openimagedenoise/lib/mkldnn_d.lib
      DEPENDEES install
    )
  endif()
endif()
