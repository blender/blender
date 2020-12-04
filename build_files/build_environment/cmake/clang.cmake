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
  -DLLVM_DIR="${LIBDIR}/llvm/lib/cmake/llvm/"
  -DLLVM_USE_CRT_RELEASE=MD
  -DLLVM_USE_CRT_DEBUG=MDd
  -DLLVM_CONFIG=${LIBDIR}/llvm/bin/llvm-config
)

set(BUILD_CLANG_TOOLS OFF)

if(WIN32)
  set(CLANG_GENERATOR "Ninja")
else()
  set(CLANG_GENERATOR "Unix Makefiles")
endif()

if(APPLE)
  set(BUILD_CLANG_TOOLS ON)
  set(CLANG_EXTRA_ARGS ${CLANG_EXTRA_ARGS}
    -DLIBXML2_LIBRARY=${LIBDIR}/xml2/lib/libxml2.a
  )
endif()

if(BUILD_CLANG_TOOLS)
  # ExternalProject_Add does not allow multiple tarballs to be
  # downloaded. Work around this by having an empty build action
  # for the extra tools, and referring the clang build to the location
  # of the clang-tools-extra source.
  ExternalProject_Add(external_clang_tools
    URL ${CLANG_TOOLS_URI}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH MD5=${CLANG_TOOLS_HASH}
    INSTALL_DIR ${LIBDIR}/clang_tools
    PREFIX ${BUILD_DIR}/clang_tools
    CONFIGURE_COMMAND echo "."
    BUILD_COMMAND echo "."
    INSTALL_COMMAND echo "."
  )
  list(APPEND CLANG_EXTRA_ARGS
    -DLLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR=${BUILD_DIR}/clang_tools/src/external_clang_tools/
  )
endif()

ExternalProject_Add(external_clang
  URL ${CLANG_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${CLANG_HASH}
  PREFIX ${BUILD_DIR}/clang
  CMAKE_GENERATOR ${CLANG_GENERATOR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/clang ${DEFAULT_CMAKE_FLAGS} ${CLANG_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/clang
)

if(MSVC)
  if(BUILD_MODE STREQUAL Release)
    set(CLANG_HARVEST_COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/ ${HARVEST_TARGET}/llvm/)
  else()
    set(CLANG_HARVEST_COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/clang/lib/ ${HARVEST_TARGET}/llvm/debug/lib/
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

if(BUILD_CLANG_TOOLS)
  # `external_clang_tools` is for downloading the source, not compiling it.
  add_dependencies(
    external_clang
    external_clang_tools
  )
endif()

# We currently do not build libxml2 on Windows.
if(NOT WIN32)
  add_dependencies(
    external_clang
    external_xml2
  )
endif()
