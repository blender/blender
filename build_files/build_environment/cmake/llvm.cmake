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

set(LLVM_EXTRA_ARGS
	-DLLVM_USE_CRT_RELEASE=MT
	-DLLVM_USE_CRT_DEBUG=MTd
	-DLLVM_INCLUDE_TESTS=OFF
	-DLLVM_TARGETS_TO_BUILD=X86
	-DLLVM_INCLUDE_EXAMPLES=OFF
	-DLLVM_ENABLE_TERMINFO=OFF
)

if(WIN32)
	set(LLVM_GENERATOR "NMake Makefiles")
else()
	set(LLVM_GENERATOR "Unix Makefiles")
endif()

# short project name due to long filename issues on windows
ExternalProject_Add(ll
	URL ${LLVM_URI}
	DOWNLOAD_DIR ${DOWNLOAD_DIR}
	URL_HASH MD5=${LLVM_HASH}
	CMAKE_GENERATOR ${LLVM_GENERATOR}
	PREFIX ${BUILD_DIR}/ll
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/llvm ${DEFAULT_CMAKE_FLAGS} ${LLVM_EXTRA_ARGS}
	INSTALL_DIR ${LIBDIR}/llvm
)

if(MSVC)
	if(BUILD_MODE STREQUAL Release)
		set(LLVM_HARVEST_COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/ ${HARVEST_TARGET}/llvm/ )
	else()
		set(LLVM_HARVEST_COMMAND
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/lib/ ${HARVEST_TARGET}/llvm/debug/lib/ &&
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/bin/ ${HARVEST_TARGET}/llvm/debug/bin/ &&
			${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/include/ ${HARVEST_TARGET}/llvm/debug/include/
		)
	endif()
	ExternalProject_Add_Step(ll after_install
		COMMAND ${LLVM_HARVEST_COMMAND}
		DEPENDEES mkdir update patch download configure build install
	)
endif()
