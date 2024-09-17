# SPDX-FileCopyrightText: 2022-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(OPENPGL_LLVM_INSTALL_PATH ${LIBDIR}/llvm)

set(OPENPGL_EXTRA_ARGS
  -DOPENPGL_BUILD_STATIC=ON
  -DOPENPGL_TBB_ROOT=${LIBDIR}/tbb
  -DTBB_ROOT=${LIBDIR}/tbb
  -DCMAKE_DEBUG_POSTFIX=_d
  -DCMAKE_CXX_COMPILER=${OPENPGL_LLVM_INSTALL_PATH}/bin/clang-cl.exe
  -DCMAKE_C_COMPILER=${OPENPGL_LLVM_INSTALL_PATH}/bin/clang-cl.exe
  -DCMAKE_C_FLAGS_INIT="--target=arm64-pc-windows-msvc"
  -DCMAKE_CXX_FLAGS_INIT="--target=arm64-pc-windows-msvc"
  -DCMAKE_SHARED_LINKER_FLAGS=-L"${LIBDIR}/../../VS1564R/Release/llvm/lib"
)

if(TBB_STATIC_LIBRARY)
  set(OPENPGL_EXTRA_ARGS
    ${OPENPGL_EXTRA_ARGS}
    -DOPENPGL_TBB_COMPONENT=tbb_static
  )
endif()

# We want the VS2019 tools for OpenPGL, as they are stable.
# We cannot use VS2022 easily, unless we specify an older (unsupported) toolset such as 17.35,
# as the newer toolsets mandate newer versions of LLVM, which we cannot use currently,
# due to lack of support in OSL and ISPC.
set(OPENPGL_VCTOOLS_REQUIRED_VERSION 14.29)

# Extract the list of installed tools that match the required version from the
# `VCToolsInstallDir` env var
file(TO_CMAKE_PATH $ENV{VCToolsInstallDir} OPENPGL_VCTOOLSINSTALLDIR_PATH)
cmake_path(GET OPENPGL_VCTOOLSINSTALLDIR_PATH PARENT_PATH OPENPGL_VCTOOLSDIR_PATH)
file(GLOB OPENPGL_INSTALLED_VCTOOLS RELATIVE ${OPENPGL_VCTOOLSDIR_PATH} ${OPENPGL_VCTOOLSDIR_PATH}/${OPENPGL_VCTOOLS_REQUIRED_VERSION}*)

# Check that at least one the installed tool versions
# (there may be different subversions) is present.
if(NOT OPENPGL_INSTALLED_VCTOOLS)
  message(FATAL_ERROR "When building for Windows ARM64 platforms, OpenPGL requires VC Tools ${OPENPGL_VCTOOLS_REQUIRED_VERSION} to be installed alongside the current version.")
endif()

# Get the last item in the list (latest, when list is sorted)
list(SORT OPENPGL_INSTALLED_VCTOOLS)
list(GET OPENPGL_INSTALLED_VCTOOLS -1 OPENPGL_VCTOOLS_VERSION)

# Configure our in file and temporarily store it in the build dir
# (with modified extension so nothing else picks it up)
# This feels icky, but boost does something similar, and we haven't called
# `ExternalProject_Add` yet, so the OpenPGL dir does not yet exist.
configure_file(
  ${PATCH_DIR}/openpgl_Directory.Build.Props.in
  ${BUILD_DIR}/openpgl_Directory.Build.Props_temp
)

# Set the patch command to copy the configured build props file in,
# and also a newer version of sse2neon
set(OPENPGL_PATCH_COMMAND
  COMMAND ${CMAKE_COMMAND} -E copy
    ${BUILD_DIR}/openpgl_Directory.Build.Props_temp
    ${BUILD_DIR}/openpgl/src/external_openpgl-build/Directory.Build.Props
  COMMAND ${CMAKE_COMMAND} -E copy
    ${BUILD_DIR}/sse2neon/src/external_sse2neon/sse2neon.h
    ${BUILD_DIR}/openpgl/src/external_openpgl/third-party/embreeSrc/common/simd/arm/sse2neon.h &&
  ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/openpgl/src/external_openpgl < ${PATCH_DIR}/openpgl_windows_arm.diff
)

# This all only works if we use the VS generator (with `clangcl` toolset), so ensure we use that
# Note: there is literally no way to get ninja to use a different toolset other than manually
# overwriting every env var, or calling a nested `vcvarsall`, both of which are *messy*.
set(OPENPGL_GENERATOR ${CMAKE_GENERATOR})
set(OPENPGL_GENERATOR_TOOLSET ClangCL)

ExternalProject_Add(external_openpgl
  URL file://${PACKAGE_DIR}/${OPENPGL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENPGL_HASH_TYPE}=${OPENPGL_HASH}
  CMAKE_GENERATOR ${OPENPGL_GENERATOR}
  CMAKE_GENERATOR_TOOLSET ${OPENPGL_GENERATOR_TOOLSET}
  PREFIX ${BUILD_DIR}/openpgl
  PATCH_COMMAND ${OPENPGL_PATCH_COMMAND}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openpgl -DCMAKE_BUILD_TYPE=${BUILD_MODE}
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENPGL_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/openpgl
)

add_dependencies(
  external_openpgl
  external_tbb
  external_sse2neon
)

if(BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_openpgl after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/openpgl
      ${HARVEST_TARGET}/openpgl

    DEPENDEES install
  )
else()
  ExternalProject_Add_Step(external_openpgl after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openpgl/lib/openpgl_d.lib
      ${HARVEST_TARGET}/openpgl/lib/openpgl_d.lib
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/openpgl/lib/cmake/openpgl-${OPENPGL_SHORT_VERSION}/openpgl_Exports-debug.cmake
      ${HARVEST_TARGET}/openpgl/lib/cmake/openpgl-${OPENPGL_SHORT_VERSION}/openpgl_Exports-debug.cmake

    DEPENDEES install
  )
endif()
