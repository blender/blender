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

if(WIN32)
	set(NUMPY_INSTALL
		${CMAKE_COMMAND} -E copy_directory "${BUILD_DIR}/python/src/external_python/run/lib/site-packages/numpy/core/include/numpy" "${LIBDIR}/python/include/python${PYTHON_SHORT_VERSION}/numpy" &&
		${CMAKE_COMMAND} -E chdir "${BUILD_DIR}/numpy/src/external_numpy/build/lib.${PYTHON_ARCH2}-${PYTHON_SHORT_VERSION}${NUMPY_DIR_POSTFIX}"
		${CMAKE_COMMAND} -E tar "cfvz" "${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}_numpy_${NUMPY_SHORT_VERSION}${NUMPY_ARCHIVE_POSTFIX}.tar.gz" "."
	)
else()
	set(NUMPY_INSTALL echo .)
	set(NUMPY_PATCH echo .)
endif()

ExternalProject_Add(external_numpy
	URL ${NUMPY_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${NUMPY_HASH}
	PREFIX ${BUILD_DIR}/numpy
	PATCH_COMMAND ${NUMPY_PATCH}
	CONFIGURE_COMMAND ""
	LOG_BUILD 1
	BUILD_COMMAND ${PYTHON_BINARY} ${BUILD_DIR}/numpy/src/external_numpy/setup.py build ${NUMPY_BUILD_OPTION} install --old-and-unmanageable
	INSTALL_COMMAND ${NUMPY_INSTALL}
)

if(WIN32)
	ExternalProject_Add_Step(external_numpy after_install
			COMMAND	${CMAKE_COMMAND} -E copy ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}_numpy_${NUMPY_SHORT_VERSION}${NUMPY_ARCHIVE_POSTFIX}.tar.gz ${HARVEST_TARGET}/Release/python${PYTHON_SHORT_VERSION_NO_DOTS}_numpy_${NUMPY_SHORT_VERSION}${NUMPY_ARCHIVE_POSTFIX}.tar.gz
			DEPENDEES install
		)
endif()

add_dependencies(
	external_numpy
	Make_Python_Environment
)
