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
    -DARM_ENABLED=Off
  )
elseif(APPLE)
  # Use bison and flex installed via Homebrew.
  # The ones that come with Xcode toolset are too old.
  if("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    set(ISPC_EXTRA_ARGS_APPLE
      -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison
      -DFLEX_EXECUTABLE=/opt/homebrew/opt/flex/bin/flex
      -DARM_ENABLED=On
    )
  else()
    set(ISPC_EXTRA_ARGS_APPLE
      -DBISON_EXECUTABLE=/usr/local/opt/bison/bin/bison
      -DARM_ENABLED=Off
    )
  endif()
elseif(UNIX)
  set(ISPC_EXTRA_ARGS_UNIX
    -DCMAKE_C_COMPILER=${LIBDIR}/llvm/bin/clang
    -DCMAKE_CXX_COMPILER=${LIBDIR}/llvm/bin/clang++
    -DARM_ENABLED=Off
  )
endif()

set(ISPC_EXTRA_ARGS
    -DISPC_NO_DUMPS=On
    -DISPC_INCLUDE_EXAMPLES=Off
    -DISPC_INCLUDE_TESTS=Off
    -DLLVM_ROOT=${LIBDIR}/llvm/lib/cmake/llvm
    -DLLVM_LIBRARY_DIR=${LIBDIR}/llvm/lib
    -DCLANG_EXECUTABLE=${LIBDIR}/llvm/bin/clang
    -DCLANGPP_EXECUTABLE=${LIBDIR}/llvm/bin/clang++
    -DISPC_INCLUDE_TESTS=Off
    -DCLANG_LIBRARY_DIR=${LIBDIR}/llvm/lib
    -DCLANG_INCLUDE_DIRS=${LIBDIR}/llvm/include
    ${ISPC_EXTRA_ARGS_WIN}
    ${ISPC_EXTRA_ARGS_APPLE}
    ${ISPC_EXTRA_ARGS_UNIX}
)

ExternalProject_Add(external_ispc
  URL file://${PACKAGE_DIR}/${ISPC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ISPC_HASH_TYPE}=${ISPC_HASH}
  PREFIX ${BUILD_DIR}/ispc
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/ispc/src/external_ispc < ${PATCH_DIR}/ispc.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ispc -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${ISPC_EXTRA_ARGS} ${BUILD_DIR}/ispc/src/external_ispc
  INSTALL_DIR ${LIBDIR}/ispc
)

add_dependencies(
  external_ispc
  ll
)

if(WIN32)
  add_dependencies(
    external_ispc
    external_flexbison
  )
endif()
