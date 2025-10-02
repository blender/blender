# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(EMBREE_CMAKE_FLAGS ${DEFAULT_CMAKE_FLAGS})

set(EMBREE_EXTRA_ARGS
  -DEMBREE_ISPC_SUPPORT=OFF
  -DEMBREE_TUTORIALS=OFF
  -DEMBREE_STATIC_LIB=OFF
  -DEMBREE_RAY_MASK=ON
  -DEMBREE_FILTER_FUNCTION=ON
  -DEMBREE_BACKFACE_CULLING=OFF
  -DEMBREE_BACKFACE_CULLING_CURVES=ON
  -DEMBREE_BACKFACE_CULLING_SPHERES=ON
  -DEMBREE_NO_SPLASH=ON
  -DEMBREE_TASKING_SYSTEM=TBB
  -DEMBREE_TBB_ROOT=${LIBDIR}/tbb
  -DTBB_ROOT=${LIBDIR}/tbb
  -DCOMPILER_HAS_SYCL_SUPPORT=OFF
)

set(EMBREE_EXTRA_ARGS
  ${EMBREE_EXTRA_ARGS}
  -DCMAKE_DEBUG_POSTFIX=_d
)

set(EMBREE_LLVM_INSTALL_PATH ${LIBDIR}/llvm)

set(EMBREE_CMAKE_FLAGS
  -DCMAKE_BUILD_TYPE=${BUILD_MODE}
)
set(EMBREE_EXTRA_ARGS
  -DCMAKE_CXX_COMPILER=${EMBREE_LLVM_INSTALL_PATH}/bin/clang-cl.exe
  -DCMAKE_C_COMPILER=${EMBREE_LLVM_INSTALL_PATH}/bin/clang-cl.exe
  -DCMAKE_C_FLAGS_INIT="--target=arm64-pc-windows-msvc"
  -DCMAKE_CXX_FLAGS_INIT="--target=arm64-pc-windows-msvc"
  -DCMAKE_SHARED_LINKER_FLAGS=-L"${LIBDIR}/llvm/lib"
  ${EMBREE_EXTRA_ARGS}
)

# We want the VS2019 tools for embree, as they are stable.
# We cannot use VS2022 easily, unless we specify an older (unsupported) toolset such as 17.35,
# as the newer toolsets mandate LLVM 16, which we cannot use currently,
# due to lack of support in OSL and ISPC.
set(EMBREE_VCTOOLS_REQUIRED_VERSION 14.29)

# Extract the list of installed tools that match the required version from the
# `VCToolsInstallDir` env var
file(TO_CMAKE_PATH $ENV{VCToolsInstallDir} EMBREE_VCTOOLSINSTALLDIR_PATH)
cmake_path(GET EMBREE_VCTOOLSINSTALLDIR_PATH PARENT_PATH EMBREE_VCTOOLSDIR_PATH)
file(GLOB EMBREE_INSTALLED_VCTOOLS RELATIVE ${EMBREE_VCTOOLSDIR_PATH} ${EMBREE_VCTOOLSDIR_PATH}/${EMBREE_VCTOOLS_REQUIRED_VERSION}*)

# Check that at least one the installed tool versions
# (there may be different subversions) is present.
if(NOT EMBREE_INSTALLED_VCTOOLS)
  message(FATAL_ERROR
    "When building for Windows ARM64 platforms, embree requires VC Tools "
    "${EMBREE_VCTOOLS_REQUIRED_VERSION} to be installed alongside the current version."
  )
endif()

# Get the last item in the list (latest, when list is sorted)
list(SORT EMBREE_INSTALLED_VCTOOLS)
list(GET EMBREE_INSTALLED_VCTOOLS -1 EMBREE_VCTOOLS_VERSION)

# Configure our in file and temporarily store it in the build dir
# (with modified extension so nothing else picks it up)
# This feels icky, but we haven't called `ExternalProject_Add` yet,
# so the embree dir does not yet exist.
configure_file(
  ${PATCH_DIR}/embree_Directory.Build.Props.in
  ${BUILD_DIR}/embree_Directory.Build.Props_temp
)

# Update the patch command to copy the configured build props file in
set(EMBREE_PATCH_COMMAND
  COMMAND ${CMAKE_COMMAND} -E copy
    ${BUILD_DIR}/embree_Directory.Build.Props_temp
    ${BUILD_DIR}/embree/src/external_embree-build/Directory.Build.Props &&
    ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/embree/src/external_embree < ${PATCH_DIR}/embree.diff
)

# This all only works if we use the VS generator (with `clangcl` toolset), so switch back to that
# Note: there is literally no way to get ninja to use a different toolset other than manually
# overwriting every env var, or calling a nested `vcvarsall`, both of which are *messy*.
set(EMBREE_GENERATOR ${CMAKE_GENERATOR})
set(EMBREE_GENERATOR_TOOLSET ClangCL)

if(TBB_STATIC_LIBRARY)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DEMBREE_TBB_COMPONENT=tbb_static
  )
endif()

ExternalProject_Add(external_embree
  URL file://${PACKAGE_DIR}/${EMBREE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${EMBREE_HASH_TYPE}=${EMBREE_HASH}
  CMAKE_GENERATOR ${EMBREE_GENERATOR}
  CMAKE_GENERATOR_TOOLSET ${EMBREE_GENERATOR_TOOLSET}
  PREFIX ${BUILD_DIR}/embree
  PATCH_COMMAND ${EMBREE_PATCH_COMMAND}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/embree ${EMBREE_CMAKE_FLAGS} ${EMBREE_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/embree
)

add_dependencies(
  external_embree
  external_tbb
  ll
)

if(BUILD_MODE STREQUAL Release)
  ExternalProject_Add_Step(external_embree after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/embree/include
      ${HARVEST_TARGET}/embree/include
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/embree/lib
      ${HARVEST_TARGET}/embree/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/embree/share
      ${HARVEST_TARGET}/embree/share
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/embree/bin/embree4.dll
      ${HARVEST_TARGET}/embree/bin/embree4.dll

    DEPENDEES install
  )
else()
  ExternalProject_Add_Step(external_embree after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/embree/bin/embree4_d.dll
      ${HARVEST_TARGET}/embree/bin/embree4_d.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/embree/lib/embree4_d.lib
      ${HARVEST_TARGET}/embree/lib/embree4_d.lib

    DEPENDEES install
  )
endif()
