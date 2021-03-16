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


ExternalProject_Add(external_openmp
  URL file://${PACKAGE_DIR}/${OPENMP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENMP_HASH_TYPE}=${OPENMP_HASH}
  PREFIX ${BUILD_DIR}/openmp
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/openmp/src/external_openmp < ${PATCH_DIR}/openmp.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openmp ${DEFAULT_CMAKE_FLAGS}
  INSTALL_COMMAND cd ${BUILD_DIR}/openmp/src/external_openmp-build && install_name_tool -id @executable_path/../Resources/lib/libomp.dylib runtime/src/libomp.dylib && make install
  INSTALL_DIR ${LIBDIR}/openmp
)

add_dependencies(
  external_openmp
  ll
)
