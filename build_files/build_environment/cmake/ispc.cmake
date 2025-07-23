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
  if(BLENDER_PLATFORM_ARM OR WITH_APPLE_CROSSPLATFORM)
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

macro(copy_host_tool_apple
  TOOL_SOURCE 
  TOOL_DEST)

  if(EXISTS ${TOOL_SOURCE})
    execute_process(COMMAND cp -rf ${TOOL_SOURCE} ${TOOL_DEST})
  else()
    message("Crosscompiled Tool: ${TOOL_SOURCE} could not be found! Please ensure that host dependencies have been built before running iOS build")
  endif()

endmacro()

if(WITH_APPLE_CROSSPLATFORM)
  # NOTE: Crosscompile currently limited due to missing curses and tinfo libs for iOS.
  #       These libs are not part of the build process, but instead we opt to statically 
  #       link and resolve indirectly via other dependencies in the project.
  # TODO: Add curses and tinfo as dependencies for ispc and build locally.

  set(LLVM_TOOLS_BINARY_DIR ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin)

  # Copy LLVM tools to build_ios path as lib dir is derived from tool dir and this erroneously links macOS libraries.
  set(LLVM_CONFIG_EXECUTABLE_PATH_ORIG ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/llvm-config)
  set(CLANG_EXECUTABLE_PATH_ORIG ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/clang)
  set(CLANGPP_EXECUTABLE_PATH_ORIG ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/clang++)
  set(LLVM_DIS_EXECUTABLE_PATH_ORIG ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/llvm-dis)
  set(LLVM_AS_EXECUTABLE_PATH_ORIG ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/llvm-as)

  set(LLVM_CONFIG_EXECUTABLE_PATH ${LIBDIR}/llvm/bin/llvm-config)
  set(CLANG_EXECUTABLE_PATH ${LIBDIR}/llvm/bin/clang)
  set(CLANGPP_EXECUTABLE_PATH ${LIBDIR}/llvm/bin/clang++)
  set(LLVM_DIS_EXECUTABLE_PATH ${LIBDIR}/llvm/bin/llvm-dis)
  set(LLVM_AS_EXECUTABLE_PATH ${LIBDIR}/llvm/bin/llvm-as)

  copy_host_tool_apple(${LLVM_CONFIG_EXECUTABLE_PATH_ORIG} ${LLVM_CONFIG_EXECUTABLE_PATH})
  copy_host_tool_apple(${CLANG_EXECUTABLE_PATH_ORIG} ${CLANG_EXECUTABLE_PATH})
  copy_host_tool_apple(${CLANGPP_EXECUTABLE_PATH_ORIG} ${CLANGPP_EXECUTABLE_PATH})
  copy_host_tool_apple(${LLVM_DIS_EXECUTABLE_PATH_ORIG} ${LLVM_DIS_EXECUTABLE_PATH})
  copy_host_tool_apple(${LLVM_AS_EXECUTABLE_PATH_ORIG} ${LLVM_AS_EXECUTABLE_PATH})

  set(ISPC_EXTRA_ARGS
    # CROSS COMPILE
    -DISPC_CROSS=ON
    -DISPC_IOS_TARGET=ON
    -DISPC_ANDROID_TARGET=OFF
    -DISPC_PS_TARGET=OFF
    -DISPC_WINDOWS_TARGET=OFF
    -DISPC_LINUX_TARGET=OFF
    -DISPC_MACOS_TARGET=OFF
    # Static
    -DISPC_STATIC_LINK=On
    #OSX_SYSROOT defaults to IOS path when building for iPad.
    -DISPC_IOS_SDK_PATH=${CMAKE_OSX_SYSROOT}
    # ISPC still needs to have access to MACOSX SDK when cross-compiling
    -DISPC_MACOS_SDK_PATH=${CMAKE_MACOSX_SYSROOT}

    -DISPC_NO_DUMPS=On
    -DISPC_INCLUDE_EXAMPLES=Off
    -DISPC_INCLUDE_TESTS=Off
    -DLLVM_ROOT=${LIBDIR}/llvm/lib/cmake/llvm
    -DLLVM_DIR=${LIBDIR}/llvm/lib/cmake/llvm
    -DLLVM_LIBRARY_DIR=${LIBDIR}/llvm/lib
    -DLLVM_INCLUDE_DIRS=${LIBDIR}/llvm/include

    # LLVM settings (Auto detect fails)
    -DLLVM_FOUND=YES
    -DLLVM_VERSION=${LLVM_VERSION}
    -DLLVM_VERSION_MAJOR=${LLVM_VERSION_MAJOR}
    -DLLVM_VERSION_MINOR=${LLVM_VERSION_MINOR}
    -DLLVM_VERSION_NUMBER=${LLVM_VERSION_NUMBER}
    -DLLVM_TARGETS_TO_BUILD=AArch64

    # Host Tools (Using freshly built tools for darwin_arm64)
    -DLLVM_TOOLS_BINARY_DIR=${LLVM_TOOLS_BINARY_DIR}
    -DLLVM_CONFIG_EXECUTABLE=${LLVM_CONFIG_EXECUTABLE_PATH}
    -DCLANG_EXECUTABLE=${CLANG_EXECUTABLE_PATH}
    -DCLANGPP_EXECUTABLE=${CLANGPP_EXECUTABLE_PATH}
    -DLLVM_DIS_EXECUTABLE=${LLVM_DIS_EXECUTABLE_PATH}
    -DLLVM_AS_EXECUTABLE=${LLVM_AS_EXECUTABLE_PATH}

    # Clang libs
    -DclangASTPath=${LIBDIR}/llvm/lib/libclangAST.a
    -DclangASTMatchersPath=${LIBDIR}/llvm/lib/libclangASTMatchers.a
    -DclangAnalysisPath=${LIBDIR}/llvm/lib/libclangAnalysis.a
    -DclangBasicPath=${LIBDIR}/llvm/lib/libclangBasic.a
    -DclangDriverPath=${LIBDIR}/llvm/lib/libclangDriver.a
    -DclangEditPath=${LIBDIR}/llvm/lib/libclangEdit.a
    -DclangFrontendPath=${LIBDIR}/llvm/lib/libclangFrontend.a
    -DclangLexPath=${LIBDIR}/llvm/lib/libclangLex.a
    -DclangParsePath=${LIBDIR}/llvm/lib/libclangParse.a
    -DclangSemaPath=${LIBDIR}/llvm/lib/libclangSema.a
    -DclangSerializationPath=${LIBDIR}/llvm/lib/libclangSerialization.a
    -DclangSupportPath=${LIBDIR}/llvm/lib/libclangSupport.a
    -DCLANG_LIBRARY_DIR=${LIBDIR}/llvm/lib
    -DCLANG_INCLUDE_DIRS=${LIBDIR}/llvm/include
    -DPython3_ROOT_DIR=${LIBDIR}/python/
    -DPython3_EXECUTABLE=${PYTHON_BINARY}
    ${ISPC_EXTRA_ARGS_APPLE}
  )
else()
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
endif()

if(WITH_APPLE_CROSSPLATFORM)
  set(ISPC_PATCH_PATH  ${PATCH_DIR}/ispc_ios.diff)
else()
  set(ISPC_PATCH_PATH  ${PATCH_DIR}/ispc.diff)
endif()

ExternalProject_Add(external_ispc
  URL file://${PACKAGE_DIR}/${ISPC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ISPC_HASH_TYPE}=${ISPC_HASH}
  PREFIX ${BUILD_DIR}/ispc

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/ispc/src/external_ispc <
    ${ISPC_PATCH_PATH}

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
