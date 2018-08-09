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

set(CLANG_EXTRA_ARGS
	-DCLANG_PATH_TO_LLVM_SOURCE=${BUILD_DIR}/ll/src/ll
	-DCLANG_PATH_TO_LLVM_BUILD=${LIBDIR}/llvm
	-DLLVM_USE_CRT_RELEASE=MT
	-DLLVM_USE_CRT_DEBUG=MTd
	-DLLVM_CONFIG=${LIBDIR}/llvm/bin/llvm-config
)
ExternalProject_Add(external_clang
	URL ${CLANG_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${CLANG_HASH}
	PREFIX ${BUILD_DIR}/clang
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/clang ${DEFAULT_CMAKE_FLAGS} ${CLANG_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/clang
)

if(MSVC)
	if(BUILD_MODE STREQUAL Release)
		set(CLANG_HARVEST_COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/ ${HARVEST_TARGET}/llvm/)
	else()
		set(CLANG_HARVEST_COMMAND
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/lib/ ${HARVEST_TARGET}/llvm/debug/lib/ &&
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/bin/ ${HARVEST_TARGET}/llvm/debug/bin/ &&
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/include/ ${HARVEST_TARGET}/llvm/debug/include/
		)
	endif()
	ExternalProject_Add_Step(external_clang after_install
		COMMAND ${CLANG_HARVEST_COMMAND}
		DEPENDEES mkdir update patch download configure build install
	)
endif()

add_dependencies(
	external_clang
	ll
)
