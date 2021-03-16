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

set(POTRACE_EXTRA_ARGS
)

if((WIN32 AND BUILD_MODE STREQUAL Release) OR UNIX)
  ExternalProject_Add(external_potrace
    URL file://${PACKAGE_DIR}/${POTRACE_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${POTRACE_HASH_TYPE}=${POTRACE_HASH}
    PREFIX ${BUILD_DIR}/potrace
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_potrace.txt ${BUILD_DIR}/potrace/src/external_potrace/CMakeLists.txt
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/potrace ${DEFAULT_CMAKE_FLAGS} ${POTRACE_EXTRA_ARGS}
    INSTALL_DIR ${LIBDIR}/potrace
  )
  if(WIN32)
    ExternalProject_Add_Step(external_potrace after_install
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/potrace ${HARVEST_TARGET}/potrace
        DEPENDEES install
    )
  endif()
endif()
