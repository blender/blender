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
if(WIN32)
	set(HARVEST_CMD cmd /C FOR /d /r ${BUILD_DIR}/python/src/external_python/run/lib/site-packages %d IN (__pycache__) DO @IF EXIST "%d" rd /s /q "%d" &&
		${CMAKE_COMMAND} -E copy_directory  ${BUILD_DIR}/python/src/external_python/run/lib/site-packages/idna ${HARVEST_TARGET}/Release/site-packages/idna &&
		${CMAKE_COMMAND} -E copy_directory  ${BUILD_DIR}/python/src/external_python/run/lib/site-packages/chardet ${HARVEST_TARGET}/Release/site-packages/chardet &&
		${CMAKE_COMMAND} -E copy_directory  ${BUILD_DIR}/python/src/external_python/run/lib/site-packages/urllib3 ${HARVEST_TARGET}/Release/site-packages/urllib3 &&
		${CMAKE_COMMAND} -E copy_directory  ${BUILD_DIR}/python/src/external_python/run/lib/site-packages/certifi ${HARVEST_TARGET}/Release/site-packages/certifi &&
		${CMAKE_COMMAND} -E copy_directory  ${BUILD_DIR}/python/src/external_python/run/lib/site-packages/requests ${HARVEST_TARGET}/Release/site-packages/requests
	)
else()
	set(HARVEST_CMD echo .)
endif()

ExternalProject_Add(external_python_site_packages
	DOWNLOAD_COMMAND ""
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	PREFIX ${BUILD_DIR}/site_packages
	INSTALL_COMMAND ${PYTHON_BINARY} -m pip install idna==${IDNA_VERSION} chardet==${CHARDET_VERSION} urllib3==${URLLIB3_VERSION} certifi==${CERTIFI_VERSION} requests==${REQUESTS_VERSION} --no-binary :all: && ${HARVEST_CMD}
)

add_dependencies(
	external_python_site_packages
	Make_Python_Environment
)
