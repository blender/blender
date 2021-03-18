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

if(APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
  set(LLVM_TARGETS AArch64$<SEMICOLON>ARM)
else()
  set(LLVM_TARGETS X86)
endif()

if(APPLE)
  set(LLVM_XML2_ARGS
    -DLIBXML2_LIBRARY=${LIBDIR}/xml2/lib/libxml2.a
  )
  set(LLVM_BUILD_CLANG_TOOLS_EXTRA ^^clang-tools-extra)
  set(BUILD_CLANG_TOOLS ON)
endif()


set(LLVM_EXTRA_ARGS
  -DLLVM_USE_CRT_RELEASE=MD
  -DLLVM_USE_CRT_DEBUG=MDd
  -DLLVM_INCLUDE_TESTS=OFF
  -DLLVM_TARGETS_TO_BUILD=${LLVM_TARGETS}
  -DLLVM_INCLUDE_EXAMPLES=OFF
  -DLLVM_ENABLE_TERMINFO=OFF
  -DLLVM_BUILD_LLVM_C_DYLIB=OFF
  -DLLVM_ENABLE_UNWIND_TABLES=OFF
  -DLLVM_ENABLE_PROJECTS=clang${LLVM_BUILD_CLANG_TOOLS_EXTRA}
  ${LLVM_XML2_ARGS}
)

if(WIN32)
  set(LLVM_GENERATOR "Ninja")
else()
  set(LLVM_GENERATOR "Unix Makefiles")
endif()

# short project name due to long filename issues on windows
ExternalProject_Add(ll
  URL file://${PACKAGE_DIR}/${LLVM_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LLVM_HASH_TYPE}=${LLVM_HASH}
  CMAKE_GENERATOR ${LLVM_GENERATOR}
  LIST_SEPARATOR ^^
  PREFIX ${BUILD_DIR}/ll
  SOURCE_SUBDIR llvm
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/ll/src/ll < ${PATCH_DIR}/llvm.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/llvm ${DEFAULT_CMAKE_FLAGS} ${LLVM_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/llvm
)

if(MSVC)
  if(BUILD_MODE STREQUAL Release)
    set(LLVM_HARVEST_COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/ ${HARVEST_TARGET}/llvm/ )
  else()
    set(LLVM_HARVEST_COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/lib/ ${HARVEST_TARGET}/llvm/debug/lib/ &&
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/llvm/include/ ${HARVEST_TARGET}/llvm/debug/include/
    )
  endif()
  ExternalProject_Add_Step(ll after_install
    COMMAND ${LLVM_HARVEST_COMMAND}
    DEPENDEES mkdir update patch download configure build install
  )
endif()

# We currently do not build libxml2 on Windows.
if(APPLE)
  add_dependencies(
    ll
    external_xml2
  )
endif()
