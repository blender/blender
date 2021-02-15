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

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  set(SITE_PACKAGES_EXTRA --global-option build --global-option --debug)
endif()

ExternalProject_Add(external_python_site_packages
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  PREFIX ${BUILD_DIR}/site_packages
  INSTALL_COMMAND ${PYTHON_BINARY} -m pip install ${SITE_PACKAGES_EXTRA} cython==${CYTHON_VERSION} idna==${IDNA_VERSION} chardet==${CHARDET_VERSION} urllib3==${URLLIB3_VERSION} certifi==${CERTIFI_VERSION} requests==${REQUESTS_VERSION} --no-binary :all:
)

if(USE_PIP_NUMPY)
  # Use only wheel (and not build from source) to stop NumPy from linking against buggy
  # Accelerate framework backend on macOS. Official wheels are built with OpenBLAS.
  ExternalProject_Add_Step(external_python_site_packages after_install
    COMMAND ${PYTHON_BINARY} -m pip install --no-cache-dir numpy==${NUMPY_VERSION} --only-binary :all:
    DEPENDEES install
  )
endif()

add_dependencies(
  external_python_site_packages
  external_python
)
