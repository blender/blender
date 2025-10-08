# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(ISPC_EXTRA_ARGS_WIN
    -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe
    -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe
    -DM4_EXECUTABLE=${DOWNLOAD_DIR}/msys2/msys64/usr/bin/m4.exe
    -DPython3_FIND_REGISTRY=NEVER
  )

  if(BLENDER_PLATFORM_ARM)
    set(ISPC_EXTRA_ARGS_WIN ${ISPC_EXTRA_ARGS_WIN} -DARM_ENABLED=On)
  else()
    set(ISPC_EXTRA_ARGS_WIN ${ISPC_EXTRA_ARGS_WIN} -DARM_ENABLED=Off)
  endif()
elseif(APPLE)
  # Use bison and flex installed via Homebrew.
  # The ones that come with Xcode toolset are too old.
  if(BLENDER_PLATFORM_ARM)
    set(ISPC_EXTRA_ARGS_APPLE
      -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison
      -DFLEX_EXECUTABLE=/opt/homebrew/opt/flex/bin/flex
      -DARM_ENABLED=On
    )
  else()
    set(ISPC_EXTRA_ARGS_APPLE
      -DBISON_EXECUTABLE=/usr/local/opt/bison/bin/bison
      -DFLEX_EXECUTABLE=/usr/local/opt/flex/bin/flex
      -DARM_ENABLED=Off
    )
  endif()
elseif(UNIX)
  set(ISPC_EXTRA_ARGS_UNIX
    -DCMAKE_C_COMPILER=${LIBDIR}/llvm/bin/clang
    -DCMAKE_CXX_COMPILER=${LIBDIR}/llvm/bin/clang++
    -DARM_ENABLED=${BLENDER_PLATFORM_ARM}
    -DFLEX_EXECUTABLE=${LIBDIR}/flex/bin/flex
  )
endif()

set(ISPC_EXTRA_ARGS
  -DISPC_NO_DUMPS=On
  -DISPC_INCLUDE_EXAMPLES=Off
  -DISPC_INCLUDE_TESTS=Off
  -DISPC_INCLUDE_RT=Off
  -DLLVM_CONFIG_EXECUTABLE=${LIBDIR}/llvm/bin/llvm-config
  -DLLVM_DIR=${LIBDIR}/llvm/lib/cmake/llvm/
  -DLLVM_LIBRARY_DIR=${LIBDIR}/llvm/lib
  -DCLANG_EXECUTABLE=${LIBDIR}/llvm/bin/clang
  -DCLANGPP_EXECUTABLE=${LIBDIR}/llvm/bin/clang++
  -DISPC_INCLUDE_TESTS=Off
  -DCLANG_LIBRARY_DIR=${LIBDIR}/llvm/lib
  -DCLANG_INCLUDE_DIRS=${LIBDIR}/llvm/include
  -DPython3_ROOT_DIR=${LIBDIR}/python/
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  ${ISPC_EXTRA_ARGS_WIN}
  ${ISPC_EXTRA_ARGS_APPLE}
  ${ISPC_EXTRA_ARGS_UNIX}
)

ExternalProject_Add(external_ispc
  URL file://${PACKAGE_DIR}/${ISPC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ISPC_HASH_TYPE}=${ISPC_HASH}
  PREFIX ${BUILD_DIR}/ispc
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/ispc/src/external_ispc <
    ${PATCH_DIR}/ispc.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/ispc
    -Wno-dev
    ${DEFAULT_CMAKE_FLAGS}
    ${ISPC_EXTRA_ARGS}
    ${BUILD_DIR}/ispc/src/external_ispc

  INSTALL_DIR ${LIBDIR}/ispc
)

add_dependencies(
  external_ispc
  ll
  external_python
)

if(WIN32)
  add_dependencies(
    external_ispc
    external_flexbison
  )
elseif(UNIX AND NOT APPLE)
  add_dependencies(
    external_ispc
    external_flex
  )
endif()
