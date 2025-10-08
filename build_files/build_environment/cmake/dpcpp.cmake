# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# LLVM does not switch over to cpp17 until llvm 16 and building earlier versions with
# MSVC is leading to some crashes in ISPC. Switch back to their default on all platforms
# for now.
string(REPLACE "-DCMAKE_CXX_STANDARD=17" " " DPCPP_CMAKE_FLAGS "${DEFAULT_CMAKE_FLAGS}")

# DPCPP already generates debug libs, there isn't much point in compiling it in debug mode itself.
string(REPLACE "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_BUILD_TYPE=Release" DPCPP_CMAKE_FLAGS "${DPCPP_CMAKE_FLAGS}")

if(WIN32)
  set(LLVM_GENERATOR "Ninja")
else()
  set(LLVM_GENERATOR "Unix Makefiles")
endif()

set(DPCPP_CONFIGURE_ARGS
  # When external deps dpcpp needs are not found it will automatically
  # download the during the configure stage using FetchContent. Given
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
  # download the during the configure stage using FetchContent. Given
  # we need to keep an archive of all source used during build for compliance
  # reasons it CANNOT download anything we do not know about. By setting
  # this property to ON, all downloads are disabled, and we will have to
  # provide the missing deps some other way, a build or configure error
  # beats a compliance violation
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DLLVMGenXIntrinsics_SOURCE_DIR=${BUILD_DIR}/vcintrinsics/src/external_vcintrinsics/
  -DOpenCL_HEADERS=file://${PACKAGE_DIR}/${OPENCLHEADERS_FILE}
  -DOpenCL_LIBRARY_SRC=file://${PACKAGE_DIR}/${ICDLOADER_FILE}
  -DSYCL_EMHASH_DIR=${LIBDIR}/emhash/include
  -DEMHASH_SYS_LOC=${LIBDIR}/emhash/include/emhash
  -DLEVEL_ZERO_LIBRARY=${LIBDIR}/level-zero/lib/${LIBPREFIX}ze_loader${SHAREDLIBEXT}
  -DLEVEL_ZERO_INCLUDE_DIR=${LIBDIR}/level-zero/include/level_zero
  -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR=${BUILD_DIR}/dpcpp_spirvheaders/src/external_dpcpp_spirvheaders/
  -DSYCL_UR_USE_FETCH_CONTENT=OFF
  -DSYCL_UR_SOURCE_DIR=${BUILD_DIR}/unifiedruntime/src/external_unifiedruntime/
  -DFETCHCONTENT_SOURCE_DIR_UNIFIED-MEMORY-FRAMEWORK=${BUILD_DIR}/unifiedmemoryframework/src/external_unifiedmemoryframework/
  -DSYCL_UMF_DISABLE_HWLOC=ON
  -DUMF_DISABLE_HWLOC=ON
  -DUMF_BUILD_SHARED_LIBRARY=OFF
  -DSYCL_ENABLE_XPTI_TRACING=ON
  -DSYCL_INCLUDE_TESTS=OFF
  -DUR_ENABLE_TRACING=ON
  -DXPTIFW_PARALLEL_HASHMAP_HEADERS=${LIBDIR}/parallelhashmap/include
  # Below here is copied from an invocation of buildbot/config.py
  -DLLVM_ENABLE_ASSERTIONS=ON
  -DLLVM_TARGETS_TO_BUILD=X86
  -DLLVM_EXTERNAL_PROJECTS=sycl^^llvm-spirv^^opencl^^libdevice^^xpti^^xptifw^^lld
  -DLLVM_EXTERNAL_SYCL_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/sycl
  -DLLVM_EXTERNAL_LLVM_SPIRV_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/llvm-spirv
  -DLLVM_EXTERNAL_XPTI_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xpti
  -DXPTI_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xpti
  -DLLVM_EXTERNAL_XPTIFW_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/xptifw
  -DLLVM_EXTERNAL_LIBDEVICE_SOURCE_DIR=${DPCPP_SOURCE_ROOT}/libdevice
  -DLLVM_ENABLE_PROJECTS=clang^^sycl^^llvm-spirv^^opencl^^libdevice^^xpti^^xptifw^^lld
  -DLIBCLC_TARGETS_TO_BUILD=
  -DLIBCLC_GENERATE_REMANGLED_VARIANTS=OFF
  -DSYCL_BUILD_PI_HIP_PLATFORM=AMD
  -DLLVM_BUILD_TOOLS=ON
  -DSYCL_ENABLE_WERROR=OFF
  -DLLVM_ENABLE_DOXYGEN=OFF
  -DLLVM_ENABLE_SPHINX=OFF
  -DBUILD_SHARED_LIBS=OFF
  -DLLVM_ENABLE_LLD=OFF
  -DXPTI_ENABLE_WERROR=OFF
  -DSYCL_CLANG_EXTRA_FLAGS=
  -DSYCL_ENABLE_PLUGINS=level_zero
  -DSYCL_ENABLE_EXTENSION_JIT=OFF
  -DSYCL_ENABLE_MAJOR_RELEASE_PREVIEW_LIB=OFF
  -DCMAKE_INSTALL_RPATH=\$ORIGIN
  -DPython3_ROOT_DIR=${LIBDIR}/python/
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
  -DLLDB_ENABLE_CURSES=OFF
  -DLLVM_ENABLE_TERMINFO=OFF
  -DLLVM_ENABLE_ZLIB=OFF
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

  PATCH_COMMAND ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/dpcpp/src/external_dpcpp <
    ${PATCH_DIR}/dpcpp.diff

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
  external_unifiedruntime
  external_unifiedmemoryframework
  external_zstd
  external_parallelhashmap
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
      COMMAND ${CMAKE_COMMAND} -E rm -f ${HARVEST_TARGET}/dpcpp/bin/lld-link.exe
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
