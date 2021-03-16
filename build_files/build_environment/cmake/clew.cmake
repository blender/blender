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

set(CLEW_EXTRA_ARGS)

ExternalProject_Add(external_clew
  URL file://${PACKAGE_DIR}/${CLEW_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${CLEW_HASH_TYPE}=${CLEW_HASH}
  PREFIX ${BUILD_DIR}/clew
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/clew -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${CLEW_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/clew
)
