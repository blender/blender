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

set(FLEXBISON_EXTRA_ARGS)

ExternalProject_Add(external_flexbison
  URL file://${PACKAGE_DIR}/${FLEXBISON_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FLEXBISON_HASH_TYPE}=${FLEXBISON_HASH}
  PREFIX ${BUILD_DIR}/flexbison
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/flexbison ${DEFAULT_CMAKE_FLAGS} ${FLEXBISON_EXTRA_ARGS}
  CONFIGURE_COMMAND echo .
  BUILD_COMMAND echo .
  INSTALL_COMMAND COMMAND ${CMAKE_COMMAND} -E copy_directory ${BUILD_DIR}/flexbison/src/external_flexbison/ ${LIBDIR}/flexbison/
  INSTALL_DIR ${LIBDIR}/flexbison
)
