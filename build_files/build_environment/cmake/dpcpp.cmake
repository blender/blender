# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

string(REPLACE "-DCMAKE_CXX_STANDARD=20" " " DPCPP_CMAKE_FLAGS "${DEFAULT_CMAKE_FLAGS}")

# DPCPP already generates debug libs, there isn't much point in compiling it in debug mode itself.
string(REPLACE "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_BUILD_TYPE=Release" DPCPP_CMAKE_FLAGS "${DPCPP_CMAKE_FLAGS}")

if(WIN32)
  set(LLVM_GENERATOR "Ninja")
else()
  set(LLVM_GENERATOR "Unix Makefiles")
endif()

set(DPCPP_CONFIGURE_ARGS
  # When external deps dpcpp needs are not found it will automatically
  # download them during the configure stage using FetchContent. Given
  # we need to keep an archive of all source used during build for compliance
  # reasons it CANNOT download anything we do not know about. By setting
  # this property to ON, all downloads are disabled, and we will have to
  # provide the missing deps some other way, a build error beats a compliance
  # violation
  --cmake-opt FETCHCONTENT_FULLY_DISCONNECTED=ON
)
set(DPCPP_SOURCE_ROOT ${BUILD_DIR}/dpcpp/src/external_dpcpp/)
set(DPCPP_EXTRA_ARGS
  # When external deps dpcpp needs are not found it will automatically
  # download them during the configure stage using FetchContent. Given
  # we need to keep an archive of all source used during build for compliance
  # reasons it CANNOT download anything we do not know about. By setting
  # this property to ON, all downloads are disabled, and we will have to
  # provide the missing deps some other way, a build or configure error
  # beats a compliance violation
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON

  # Pre-built dependency paths and other Blender-specific settings.
  # These are not produced by configure.py. They mainly redirect
  # dependencies to pre-populated source trees instead of letting the
  # DPC++ build system fetch them, which is required for our offline
  # and compliance constraints. Some of them depend on our dpcpp.diff
  # changes to work correctly.
  -DFETCHCONTENT_SOURCE_DIR_VC-INTRINSICS=${BUILD_DIR}/vcintrinsics/src/external_vcintrinsics/
  -DFETCHCONTENT_SOURCE_DIR_OCL-HEADERS=${BUILD_DIR}/openclheaders/src/external_openclheaders
  -DFETCHCONTENT_SOURCE_DIR_OCL-ICD=${BUILD_DIR}/icdloader/src/external_icdloader
  -DFETCHCONTENT_SOURCE_DIR_HWLOC_TARG=${BUILD_DIR}/hwloc/src/external_hwloc
  # DPC++ uses find_package(emhash QUIET). external_emhash installs a proper CMake
  # package to ${LIBDIR}/emhash, so just make it discoverable.
  -DCMAKE_PREFIX_PATH=${LIBDIR}/emhash
  -DLEVEL_ZERO_LIBRARY=${LIBDIR}/level-zero/lib/${LIBPREFIX}ze_loader${SHAREDLIBEXT}
  -DLEVEL_ZERO_INCLUDE_DIR=${LIBDIR}/level-zero/include/level_zero
  -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR=${BUILD_DIR}/dpcpp_spirvheaders/src/external_dpcpp_spirvheaders/
  -DFETCHCONTENT_SOURCE_DIR_UNIFIED-MEMORY-FRAMEWORK=${BUILD_DIR}/unifiedmemoryframework/src/external_unifiedmemoryframework/
  -DUMF_LINK_HWLOC_STATICALLY=ON
  -DUMF_BUILD_SHARED_LIBRARY=OFF
  # These two options are from configure.py, but they are left here for now for the better
  # readability of the changes - they would be moved downward in the future refactoring.
  -DSYCL_ENABLE_XPTI_TRACING=ON
  # [CHANGED] We do not need to build tests for our purposes
  -DSYCL_INCLUDE_TESTS=OFF
  # sycl/CMakeLists.txt calls add_subdirectory(test-e2e) unconditionally, i.e.
  # it is NOT gated by SYCL_INCLUDE_TESTS. test-e2e/CMakeLists.txt then runs
  # find_package(CUDAToolkit), which would pick up a CUDA toolkit from the host
  # machine - an undeclared dependency we do not want. The lookup is guarded by
  # `if(NOT DEFINED CUDA_LIBS_DIR AND NOT DEFINED CUDA_INCLUDE)`, so defining
  # these suppresses it.
  -DCUDA_INCLUDE=
  -DCUDA_LIBS_DIR=
  -DUR_ENABLE_TRACING=ON
  -DXPTIFW_PARALLEL_HASHMAP_HEADERS=${LIBDIR}/parallelhashmap/include
  -DPHMAP_LOC=${LIBDIR}/parallelhashmap/include/parallel_hashmap
  # As we are downloading a tarball and not a git repository, we need to
  # set the value for this variable manually to the proper version
  # ourselves, which is the timestamp of the latest commit in the tag or
  # branch which is used.
  # In our case: 880399f47228ac92f403e3b2b865cf1d6f0b1382
  -DSYCL_COMPILER_VERSION=20260831

  # Below here is copied from an invocation of buildbot/configure.py with
  # arguments:
  # "--use-zstd --disable-preview-lib --disable-jit --host-target=x86 --print-cmake-flags"
  #
  # The original generated arguments were this:
  # >"C:\db\build\output\Win64_vc17\python\313\bin\python.exe" buildbot\configure.py --use-zstd --disable-preview-lib --disable-jit --host-target=x86 --print-cmake-flags
  #   -G Ninja
  #   -DCMAKE_BUILD_TYPE=Release
  #   -DLLVM_ENABLE_ASSERTIONS=ON
  #   '-DLLVM_TARGETS_TO_BUILD=x86;SPIRV'
  #   '-DLLVM_EXTERNAL_PROJECTS=sycl;llvm-spirv;opencl;xpti;xptifw;compiler-rt;libdevice'
  #   '-DLLVM_EXTERNAL_SYCL_SOURCE_DIR=C:\Repos\llvm-7.1.0\sycl'
  #   '-DLLVM_EXTERNAL_LLVM_SPIRV_SOURCE_DIR=C:\Repos\llvm-7.1.0\llvm-spirv'
  #   '-DLLVM_EXTERNAL_XPTI_SOURCE_DIR=C:\Repos\llvm-7.1.0\xpti'
  #   '-DXPTI_SOURCE_DIR=C:\Repos\llvm-7.1.0\xpti'
  #   '-DLLVM_EXTERNAL_XPTIFW_SOURCE_DIR=C:\Repos\llvm-7.1.0\xptifw'
  #   '-DLLVM_EXTERNAL_LIBDEVICE_SOURCE_DIR=C:\Repos\llvm-7.1.0\libdevice'
  #   '-DLLVM_EXTERNAL_SYCL_JIT_SOURCE_DIR=C:\Repos\llvm-7.1.0\sycl-jit'
  #   '-DLLVM_ENABLE_PROJECTS=clang;sycl;llvm-spirv;opencl;xpti;xptifw;compiler-rt;libdevice;lld'
  #   -DSYCL_BUILD_PI_HIP_PLATFORM=AMD
  #   -DLLVM_BUILD_TOOLS=ON
  #   -DLLVM_ENABLE_ZSTD=FORCE_ON
  #   -DLLVM_USE_STATIC_ZSTD=ON
  #   -DSYCL_ENABLE_WERROR=OFF
  #   '-DCMAKE_INSTALL_PREFIX=C:\Repos\llvm-7.1.0\build\install'
  #   -DSYCL_INCLUDE_TESTS=ON
  #   -DLLVM_ENABLE_DOXYGEN=OFF
  #   -DLLVM_ENABLE_SPHINX=OFF
  #   -DBUILD_SHARED_LIBS=OFF
  #   -DSYCL_ENABLE_XPTI_TRACING=ON
  #   -DLLVM_ENABLE_LLD=OFF
  #   -DLLVM_SPIRV_ENABLE_LIBSPIRV_DIS=OFF
  #   -DXPTI_ENABLE_WERROR=OFF
  #   -DSYCL_CLANG_EXTRA_FLAGS=
  #   '-DSYCL_ENABLE_BACKENDS=opencl;level_zero_v2;level_zero'
  #   -DSYCL_ENABLE_EXTENSION_JIT=OFF
  #   -DSYCL_ENABLE_MAJOR_RELEASE_PREVIEW_LIB=OFF
  #   -DBUG_REPORT_URL=https://github.com/intel/llvm/issues
  #   'C:\Repos\llvm-7.1.0\llvm'
  #   --print-cmake-flags
  #  >
  #
  # All cmake parameters from the list above, which we set with
  # non-default options are marked as [CHANGED]. Other parameters,
  # not from the list above do not have this mark regardless of
  # their values.

  # -DCMAKE_BUILD_TYPE=Release              # set by ExternalProject_Add
  -DLLVM_ENABLE_ASSERTIONS=ON
  -DLLVM_TARGETS_TO_BUILD=X86^^SPIRV
  # Note: lld/clang are in-tree LLVM projects, only listed in LLVM_ENABLE_PROJECTS.
  -DLLVM_EXTERNAL_PROJECTS=sycl^^llvm-spirv^^opencl^^xpti^^xptifw^^compiler-rt^^libdevice
  -DLLVM_EXTERNAL_SYCL_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/sycl
  -DLLVM_EXTERNAL_LLVM_SPIRV_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/llvm-spirv
  -DLLVM_EXTERNAL_XPTI_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xpti
  -DXPTI_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xpti
  -DLLVM_EXTERNAL_XPTIFW_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xptifw
  -DLLVM_EXTERNAL_LIBDEVICE_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/libdevice
  -DLLVM_EXTERNAL_SYCL_JIT_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/sycl-jit
  -DLLVM_ENABLE_PROJECTS=clang^^sycl^^llvm-spirv^^opencl^^xpti^^xptifw^^compiler-rt^^libdevice^^lld

  -DSYCL_BUILD_PI_HIP_PLATFORM=AMD
  -DLLVM_BUILD_TOOLS=ON
  -DSYCL_ENABLE_WERROR=OFF
  # -DCMAKE_INSTALL_PREFIX=...              # set by ExternalProject_Add
  # [CHANGED] Tests disabled (configure.py default: ON) and handled above.
  -DLLVM_ENABLE_DOXYGEN=OFF
  -DLLVM_ENABLE_SPHINX=OFF
  -DBUILD_SHARED_LIBS=OFF
  -DLLVM_ENABLE_LLD=OFF
  -DLLVM_SPIRV_ENABLE_LIBSPIRV_DIS=OFF
  -DXPTI_ENABLE_WERROR=OFF
  -DSYCL_CLANG_EXTRA_FLAGS=
  # [CHANGED] Removed opencl backend; we only target Level Zero.
  # configure.py default: opencl;level_zero_v2;level_zero
  # Previously this option was called SYCL_ENABLE_PLUGINS in DPC++ 6.x
  -DSYCL_ENABLE_BACKENDS=level_zero_v2^^level_zero
  -DSYCL_ENABLE_EXTENSION_JIT=OFF
  -DSYCL_ENABLE_MAJOR_RELEASE_PREVIEW_LIB=OFF
  # -DBUG_REPORT_URL=...                    # omitted, cosmetic

  # Additional project-specific flags (not from configure.py)
  -DCMAKE_INSTALL_RPATH=\$ORIGIN
  -DPython3_ROOT_DIR=${LIBDIR}/python/
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
  -DLLVM_ENABLE_ZLIB=OFF
  # The following two are from configure.py, but left here for now for better
  # readability of the changes - they will be moved upward in future refactoring.
  -DLLVM_ENABLE_ZSTD=FORCE_ON
  -DLLVM_USE_STATIC_ZSTD=ON
  -Dzstd_INCLUDE_DIR=${LIBDIR}/zstd/include
)

if(WIN32)
  list(APPEND DPCPP_EXTRA_ARGS
    -DPython3_FIND_REGISTRY=NEVER
    -Dzstd_LIBRARY=${LIBDIR}/zstd/lib/zstd_static.lib
  )
else()
  list(APPEND DPCPP_EXTRA_ARGS
    -Dzstd_LIBRARY=${LIBDIR}/zstd/lib/libzstd.a
  )
endif()

ExternalProject_Add(external_dpcpp
  URL file://${PACKAGE_DIR}/${DPCPP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${DPCPP_HASH_TYPE}=${DPCPP_HASH}
  PREFIX ${BUILD_DIR}/dpcpp
  CMAKE_GENERATOR ${LLVM_GENERATOR}
  SOURCE_SUBDIR llvm
  LIST_SEPARATOR ^^

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/dpcpp
    ${DPCPP_CMAKE_FLAGS}
    ${DPCPP_EXTRA_ARGS}

  # CONFIGURE_COMMAND
  #   ${PYTHON_BINARY}
  #   ${BUILD_DIR}/dpcpp/src/external_dpcpp/buildbot/configure.py ${DPCPP_CONFIGURE_ARGS}
  # BUILD_COMMAND
  #   echo "." # ${PYTHON_BINARY} ${BUILD_DIR}/dpcpp/src/external_dpcpp/buildbot/compile.py
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . -- deploy-sycl-toolchain

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/dpcpp/src/external_dpcpp <
      ${PATCH_DIR}/dpcpp.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/dpcpp/src/external_dpcpp <
      ${PATCH_DIR}/dpcpp_backport_23007.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/dpcpp/src/external_dpcpp <
      ${PATCH_DIR}/dpcpp_backport_23028.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/dpcpp/src/external_dpcpp <
      ${PATCH_DIR}/dpcpp_backport_23012.diff &&
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/dpcpp/src/external_dpcpp <
      ${PATCH_DIR}/dpcpp_neo_dependency_adoption.diff

  INSTALL_DIR ${LIBDIR}/dpcpp
)

add_dependencies(
  external_dpcpp
  external_python
  external_python_site_packages
  external_vcintrinsics
  external_openclheaders
  external_icdloader
  external_emhash
  external_level-zero
  external_dpcpp_spirvheaders
  external_unifiedmemoryframework
  external_zstd
  external_parallelhashmap
  external_hwloc
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_dpcpp after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/dpcpp ${HARVEST_TARGET}/dpcpp
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/clang-cl.exe
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/clang-cpp.exe
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/ld.lld.exe
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/ld64.lld.exe
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/lld.exe
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/wasm-ld.exe
      DEPENDEES install
    )
  endif()
else()
  harvest(external_dpcpp dpcpp/bin dpcpp/bin "*")
  harvest(external_dpcpp dpcpp/include dpcpp/include "*")
  harvest(external_dpcpp dpcpp/lib dpcpp/lib "libsycl*")
  harvest(external_dpcpp dpcpp/lib dpcpp/lib "libxpti*")
  harvest(external_dpcpp dpcpp/lib dpcpp/lib "libur*")
  harvest(external_dpcpp dpcpp/lib/clang dpcpp/lib/clang "*")
endif()
