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
	if(BUILD_MODE STREQUAL Release)
		set(NUMPY_POSTFIX)
		message("Python_binary = ${PYTHON_BINARY}")
		message("Python_post = ${PYTHON_POSTFIX}")

		ExternalProject_Add(external_numpy
			URL ${NUMPY_URI}
			DOWNLOAD_DIR ${DOWNLOAD_DIR}
			URL_HASH MD5=${NUMPY_HASH}
			PREFIX ${BUILD_DIR}/numpy
			PATCH_COMMAND ${PATCH_CMD}	--verbose -p 1 -N -d ${BUILD_DIR}/numpy/src/external_numpy < ${PATCH_DIR}/numpy.diff
			CONFIGURE_COMMAND ""
			LOG_BUILD 1
			BUILD_COMMAND ${PYTHON_BINARY} ${BUILD_DIR}/numpy/src/external_numpy/setup.py build
			INSTALL_COMMAND ${CMAKE_COMMAND} -E chdir "${BUILD_DIR}/numpy/src/external_numpy/build/lib.${PYTHON_ARCH2}-3.5"
							${CMAKE_COMMAND} -E tar "cfvz" "${LIBDIR}/python35_numpy${PYTHON_POSTFIX}_1.11.tar.gz" "."
		)

		add_dependencies(external_numpy Make_Python_Environment)
	endif()
endif()
