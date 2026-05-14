# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ISPC_EXTRA_ARGS
  -DISPC_INCLUDE_EXAMPLES=OFF
  -DISPC_INCLUDE_TESTS=OFF
  -DISPC_INCLUDE_RT=OFF
  -DISPC_INCLUDE_UTILS=OFF
  -DISPC_LIBRARY=OFF
  -DLLVM_CONFIG_EXECUTABLE=${LIBDIR}/llvm/bin/llvm-config
  -DLLVM_DIR=${LIBDIR}/llvm/lib/cmake/llvm/
  -DCLANG_EXECUTABLE=${LIBDIR}/llvm/bin/clang
  -DCLANGPP_EXECUTABLE=${LIBDIR}/llvm/bin/clang++
  -DPython3_ROOT_DIR=${LIBDIR}/python/
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DGIT_BINARY=GIT_BINARY-NOTFOUND # Prevent any git checks
)

if(WIN32)
  list(APPEND ISPC_EXTRA_ARGS
    -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe
    -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe
    -DM4_EXECUTABLE=${DOWNLOAD_DIR}/msys2/msys64/usr/bin/m4.exe
    -DPython3_FIND_REGISTRY=NEVER
  )

  if(BLENDER_PLATFORM_ARM)
    list(APPEND ISPC_EXTRA_ARGS -DARM_ENABLED=ON)
  else()
    list(APPEND ISPC_EXTRA_ARGS -DARM_ENABLED=OFF)
  endif()
elseif("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
  # Use bison and flex installed via Homebrew.
  # The ones that come with Xcode toolset are too old.
  list(APPEND ISPC_EXTRA_ARGS
    -DBISON_EXECUTABLE=/opt/homebrew/opt/bison/bin/bison
    -DFLEX_EXECUTABLE=/opt/homebrew/opt/flex/bin/flex
    -DARM_ENABLED=ON
  )
elseif(UNIX)
  list(APPEND ISPC_EXTRA_ARGS
    -DCMAKE_C_COMPILER=gcc
    -DCMAKE_CXX_COMPILER=g++
    -DARM_ENABLED=${BLENDER_PLATFORM_ARM}
    -DFLEX_EXECUTABLE=${LIBDIR}/flex/bin/flex
  )
endif()

# Cross-compilation target support
# Disambiguation note: Compiling ISPC on the host *WITH* extra cross-compilation target available, not cross-compiling ISPC itself.
# Only supporting macOS due to some bad intrisic to find the Android NDK, but Linux wouldn't require much changes.
if(APPLE)
  list(APPEND ISPC_EXTRA_ARGS
    -DISPC_CROSS=ON

    -DISPC_WINDOWS_TARGET=OFF
    -DISPC_LINUX_TARGET=OFF
    -DISPC_FREEBSD_TARGET=OFF
    -DISPC_MACOS_TARGET=OFF
    -DISPC_ANDROID_TARGET=OFF
    -DISPC_PS_TARGET=OFF

    -DISPC_IOS_TARGET=OFF
    -DISPC_ANDROID_TARGET=OFF
  )

  # If building on macOS, pre-emptively build with iOS cross-compilation support for an eventual iOS deps build.
  if(APPLE)
    list(APPEND ISPC_EXTRA_ARGS
      -DISPC_IOS_TARGET=ON
    )
  endif()

  # Android cross-compilation requires an ANDROID_NDK root to be passed, after which this ISPC can be used for
  # an Android deps build.
  # TODO: Replace by better heuristics or use an if(DEFINED) and allow passing extra CMake args to make deps
  set(ISPC_ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk/30.0.14904198/)
  if(EXISTS ${ISPC_ANDROID_NDK})
    message(NOTICE "ISPC: Building with Android cross-compilation support, NDK: ${ISPC_ANDROID_NDK}")
    list(APPEND ISPC_EXTRA_ARGS
      -DISPC_ANDROID_TARGET=ON
      -DISPC_ANDROID_NDK_PATH=${ISPC_ANDROID_NDK}
    )
  else()
    message(WARNING "ISPC: Android NDK not found, ISPC won't be built with Android cross-compilation support")
  endif()
endif()

ExternalProject_Add(external_ispc
  URL file://${PACKAGE_DIR}/${ISPC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ISPC_HASH_TYPE}=${ISPC_HASH}
  PREFIX ${BUILD_DIR}/ispc
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/ispc/src/external_ispc <
      ${PATCH_DIR}/ispc.diff &&
    # For Android target cross-compiling support: Update the Android NDK include sysroot, changed in NDK r19+.
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/ispc/src/external_ispc <
      ${PATCH_DIR}/ispc_crosscompile_android.diff

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
  external_llvm
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
