# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(BLENDER_PLATFORM_ARM)
  set(LLVM_TARGETS AArch64$<SEMICOLON>ARM)
else()
  set(LLVM_TARGETS X86)
endif()

if(UNIX AND NOT APPLE)
  # Make llvm's pkgconfig pick up our static xml2 lib
  set(LLVM_XML2_ARGS
    -DCMAKE_PREFIX_PATH=${LIBDIR}/xml2
  )
endif()

if(APPLE)
  set(LLVM_XML2_ARGS
    -DLIBXML2_LIBRARY=${LIBDIR}/xml2/lib/libxml2.a
    -DLIBXML2_INCLUDE_DIR=${LIBDIR}/xml2/include/libxml2
  )
  set(LLVM_EXTRA_PROJECTS ^^clang-tools-extra)
  set(BUILD_CLANG_TOOLS ON)
elseif(MSVC AND BLENDER_PLATFORM_ARM)
  set(LLVM_EXTRA_PROJECTS ^^lld)
else()
  # NVIDIA PTX for OSL on Windows and Linux.
  set(LLVM_TARGETS ${LLVM_TARGETS}$<SEMICOLON>NVPTX)
endif()

if(WITH_APPLE_CROSSPLATFORM)
  # Specify host machine Python and LLVM tools, to prevent local building.
  # WIP: LLVM still tries to compile Tablegen for host machine ):
  message("Setting Crosscompilation tools dir for LLVM: ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}")
  set(LLVM_PYTHON_ROOT_DIR ${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/python)
  set(LLVM_TOOL_COMPILE_ARGS
    -DPython3_ROOT_DIR=${LLVM_PYTHON_ROOT_DIR}
    -DPython3_EXECUTABLE=${LLVM_PYTHON_ROOT_DIR}/bin/python3
    -DLLVM_USE_HOST_TOOLS=ON
    -DLLVM_TOOLS_BINARY_DIR=${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin
    -DLLVM_NATIVE_TOOL_DIR=${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin
    -DLLVM_INCLUDE_RUNTIMES=ON
    -DLLVM_TARGETS_TO_BUILD=AArch64$<SEMICOLON>ARM
    -DLLVM_ENABLE_BACKTRACES=Off
    -DLLVM_OPTIMIZED_TABLEGEN=OFF
    -DLLVM_TABLEGEN_EXE:string=${CMAKE_DEPS_CROSSCOMPILE_BUILDDIR}/deps_arm64/Release/llvm/bin/llvm-tblgen
    -DLLVM_ENABLE_PROJECTS=clang${LLVM_BUILD_CLANG_TOOLS_EXTRA}
    -DLLVM_ENABLE_PROJECTS_USED=ON
  )
else()
  set(LLVM_TOOL_COMPILE_ARGS
    -DPython3_ROOT_DIR=${LIBDIR}/python/
    -DPython3_EXECUTABLE=${PYTHON_BINARY}
    -DLLVM_ENABLE_PROJECTS=clang${LLVM_BUILD_CLANG_TOOLS_EXTRA}
    -DLLVM_TARGETS_TO_BUILD=${LLVM_TARGETS}
  )
endif()

set(LLVM_EXTRA_ARGS
  -DLLVM_USE_CRT_RELEASE=MD
  -DLLVM_USE_CRT_DEBUG=MDd
  -DLLVM_INCLUDE_TESTS=OFF
  -DLLVM_INCLUDE_EXAMPLES=OFF
  -DLLVM_ENABLE_TERMINFO=OFF
  -DLLVM_BUILD_LLVM_C_DYLIB=OFF
  -DLLVM_ENABLE_UNWIND_TABLES=OFF
  -DLLVM_ENABLE_ZSTD=OFF
  -DLLVM_ENABLE_ZLIB=OFF
  -DLLVM_ENABLE_PROJECTS=clang${LLVM_EXTRA_PROJECTS}
  -DPython3_ROOT_DIR=${LIBDIR}/python/
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  ${LLVM_XML2_ARGS}
)

if(WIN32)
  set(LLVM_GENERATOR "Ninja")
  list(APPEND LLVM_EXTRA_ARGS -DPython3_FIND_REGISTRY=NEVER)
else()
  set(LLVM_GENERATOR "Unix Makefiles")
endif()

# LLVM does not switch over to cpp17 until llvm 16 and building ealier versions with
# MSVC is leading to some crashes in ISPC. Switch back to their default on all platforms
# for now.
string(REPLACE "-DCMAKE_CXX_STANDARD=17" " " LLVM_CMAKE_FLAGS "${DEFAULT_CMAKE_FLAGS}")

if(WITH_APPLE_CROSSPLATFORM)
  set(LLVM_PATCH_DIFF ${PATCH_DIR}/llvm_ios.diff)
else()
  set(LLVM_PATCH_DIFF ${PATCH_DIR}/llvm.diff)
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

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/ll/src/ll <
    ${LLVM_PATCH_DIFF}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/llvm
    ${LLVM_TOOL_COMPILE_ARGS}
    ${LLVM_CMAKE_FLAGS}
    ${LLVM_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/llvm
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    set(LLVM_HARVEST_COMMAND
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/llvm/lib
        ${HARVEST_TARGET}/llvm/lib &&
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/llvm/include
        ${HARVEST_TARGET}/llvm/include &&
      ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/llvm/bin/clang-format.exe
        ${HARVEST_TARGET}/llvm/bin/clang-format.exe
    )
  else()
    set(LLVM_HARVEST_COMMAND
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/llvm/lib/
        ${HARVEST_TARGET}/llvm/debug/lib/ &&
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/llvm/include/
        ${HARVEST_TARGET}/llvm/debug/include/
    )
  endif()
  ExternalProject_Add_Step(ll after_install
    COMMAND ${LLVM_HARVEST_COMMAND}
    DEPENDEES mkdir update patch download configure build install
  )
else()
  harvest(ll llvm/bin llvm/bin "clang-format")
  if(BUILD_CLANG_TOOLS)
    harvest(ll llvm/bin llvm/bin "clang-tidy")
    harvest(ll llvm/share/clang llvm/share "run-clang-tidy.py")
  endif()
  harvest(ll llvm/include llvm/include "*")
  harvest(ll llvm/bin llvm/bin "llvm-config")
  harvest(ll llvm/lib llvm/lib "libLLVM*.a")
  harvest(ll llvm/lib llvm/lib "libclang*.a")
  harvest(ll llvm/lib/clang llvm/lib/clang "*.h")
endif()

# We currently do not build libxml2 on Windows.
if(UNIX)
  add_dependencies(
    ll
    external_xml2
  )
endif()

if(NOT WITH_APPLE_CROSSPLATFORM)
  # If building for IOS, use host python.
  add_dependencies(
    ll
    external_python
  )
endif()