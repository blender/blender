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

if(MSVC)
  message("BIN >${PYTHON_BINARY}<")
  if(BUILD_MODE STREQUAL Debug)
    set(NUMPY_DIR_POSTFIX -pydebug)
    set(NUMPY_ARCHIVE_POSTFIX d)
    set(NUMPY_BUILD_OPTION --debug)
  else()
    set(NUMPY_DIR_POSTFIX)
    set(NUMPY_ARCHIVE_POSTFIX)
    set(NUMPY_BUILD_OPTION)
  endif()
endif()

set(NUMPY_POSTFIX)

ExternalProject_Add(external_numpy
  URL file://${PACKAGE_DIR}/${NUMPY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NUMPY_HASH_TYPE}=${NUMPY_HASH}
  PREFIX ${BUILD_DIR}/numpy
  PATCH_COMMAND ${NUMPY_PATCH}
  CONFIGURE_COMMAND ""
  LOG_BUILD 1
  BUILD_COMMAND ${PYTHON_BINARY} ${BUILD_DIR}/numpy/src/external_numpy/setup.py build ${NUMPY_BUILD_OPTION} install --old-and-unmanageable
  INSTALL_COMMAND ""
)

add_dependencies(
  external_numpy
  external_python
  external_python_site_packages
)
