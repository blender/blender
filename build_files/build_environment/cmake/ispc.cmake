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
  set(ISPC_EXTRA_ARGS_WIN
    -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe
    -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe
    -DM4_EXECUTABLE=${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/m4.exe
  )
endif()

set(ISPC_EXTRA_ARGS
    -DARM_ENABLED=Off
    -DISPC_NO_DUMPS=On
    -DISPC_INCLUDE_EXAMPLES=Off
    -DISPC_INCLUDE_TESTS=Off
    -DLLVM_ROOT=${LIBDIR}/llvm/lib/cmake/llvm
    -DCLANG_EXECUTABLE=${LIBDIR}/clang/bin/clang
    -DISPC_INCLUDE_TESTS=Off
    -DCLANG_LIBRARY_DIR=${LIBDIR}/clang/lib
    -DCLANG_INCLUDE_DIRS=${LIBDIR}/clang/include
    ${ISPC_EXTRA_ARGS_WIN}
)

ExternalProject_Add(external_ispc
  URL ${ISPC_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${ISPC_HASH}
  PREFIX ${BUILD_DIR}/ispc
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/ispc/src/external_ispc < ${PATCH_DIR}/ispc.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ispc -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${ISPC_EXTRA_ARGS} ${BUILD_DIR}/ispc/src/external_ispc
  INSTALL_DIR ${LIBDIR}/ispc
)

add_dependencies(
  external_ispc
  ll
  external_clang
)

if(WIN32)
  add_dependencies(
    external_ispc
    external_flexbison
  )
endif()

