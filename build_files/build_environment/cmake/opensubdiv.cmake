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

set(OPENSUBDIV_EXTRA_ARGS
	-DNO_EXAMPLES=ON
	-DNO_REGRESSION=ON
	-DNO_PYTHON=ON
	-DNO_MAYA=ON
	-DNO_PTEX=ON
	-DNO_DOC=ON
	-DNO_CLEW=OFF
	-DNO_OPENCL=OFF
	-DNO_TUTORIALS=ON
	-DGLEW_INCLUDE_DIR=${LIBDIR}/glew/include
	-DGLEW_LIBRARY=${LIBDIR}/glew/lib/libGLEW${LIBEXT}
	-DGLFW_INCLUDE_DIR=${LIBDIR}/glfw/include
	-DGLFW_LIBRARIES=${LIBDIR}/glfw/lib/glfw3${LIBEXT}
)

if(WIN32)
	#no cuda support for vc15 yet
	if(msvc15)
		set(OPENSUBDIV_CUDA ON)
	else()
		set(OPENSUBDIV_CUDA ON)
	endif()

	set(OPENSUBDIV_EXTRA_ARGS
		${OPENSUBDIV_EXTRA_ARGS}
		-DNO_CUDA=${OPENSUBDIV_CUDA}
		-DCLEW_INCLUDE_DIR=${LIBDIR}/clew/include/CL
		-DCLEW_LIBRARY=${LIBDIR}/clew/lib/clew${LIBEXT}
		-DCUEW_INCLUDE_DIR=${LIBDIR}/cuew/include
		-DCUEW_LIBRARY=${LIBDIR}/cuew/lib/cuew${LIBEXT}
		-DCMAKE_EXE_LINKER_FLAGS_RELEASE=libcmt.lib
	)
else()
	set(OPENSUBDIV_EXTRA_ARGS
		${OPENSUBDIV_EXTRA_ARGS}
		-DNO_CUDA=ON
		-DCUEW_INCLUDE_DIR=${LIBDIR}/cuew/include
		-DCLEW_INCLUDE_DIR=${LIBDIR}/clew/include/CL
		-DCLEW_LIBRARY=${LIBDIR}/clew/lib/static/${LIBPREFIX}clew${LIBEXT}
	)
endif()

ExternalProject_Add(external_opensubdiv
	URL ${OPENSUBDIV_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${OPENSUBDIV_Hash}
	PREFIX ${BUILD_DIR}/opensubdiv
	PATCH_COMMAND ${PATCH_CMD} --verbose -p 1 -N -d ${BUILD_DIR}/opensubdiv/src/external_opensubdiv < ${PATCH_DIR}/opensubdiv.diff
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opensubdiv -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${OPENSUBDIV_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/opensubdiv
)

add_dependencies(
	external_opensubdiv
	external_glew
	external_glfw
	external_clew
	external_cuew
)
