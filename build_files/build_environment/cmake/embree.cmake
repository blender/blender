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
)

if(WIN32)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DCMAKE_DEBUG_POSTFIX=_d
  )
endif()

if(NOT BLENDER_PLATFORM_ARM)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DEMBREE_MAX_ISA=AVX2
  )
endif()

if(NOT APPLE AND NOT BLENDER_PLATFORM_ARM)
  if(WIN32)
    # Levels below -O2 don't work well for Embree+SYCL.
    string(REGEX REPLACE "-O[A-Za-z0-9]" "" EMBREE_CLANG_CMAKE_CXX_FLAGS_DEBUG ${BLENDER_CLANG_CMAKE_C_FLAGS_DEBUG})
    string(APPEND EMBREE_CLANG_CMAKE_CXX_FLAGS_DEBUG " -O2")
    string(REGEX REPLACE "-O[A-Za-z0-9]" "" EMBREE_CLANG_CMAKE_C_FLAGS_DEBUG ${BLENDER_CLANG_CMAKE_C_FLAGS_DEBUG})
    string(APPEND EMBREE_CLANG_CMAKE_C_FLAGS_DEBUG " -O2")
    set(EMBREE_CMAKE_FLAGS
      -DCMAKE_BUILD_TYPE=${BUILD_MODE}
      -DCMAKE_CXX_FLAGS_RELEASE=${BLENDER_CLANG_CMAKE_CXX_FLAGS_RELEASE}
      -DCMAKE_CXX_FLAGS_MINSIZEREL=${BLENDER_CLANG_CMAKE_CXX_FLAGS_MINSIZEREL}
      -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${BLENDER_CLANG_CMAKE_CXX_FLAGS_RELWITHDEBINFO}
      -DCMAKE_CXX_FLAGS_DEBUG=${EMBREE_CLANG_CMAKE_CXX_FLAGS_DEBUG}
      -DCMAKE_C_FLAGS_RELEASE=${BLENDER_CLANG_CMAKE_C_FLAGS_RELEASE}
      -DCMAKE_C_FLAGS_MINSIZEREL=${BLENDER_CLANG_CMAKE_C_FLAGS_MINSIZEREL}
      -DCMAKE_C_FLAGS_RELWITHDEBINFO=${BLENDER_CLANG_CMAKE_C_FLAGS_RELWITHDEBINFO}
      -DCMAKE_C_FLAGS_DEBUG=${EMBREE_CLANG_CMAKE_C_FLAGS_DEBUG}
      -DCMAKE_CXX_STANDARD=17
    )
    set(EMBREE_EXTRA_ARGS
      -DCMAKE_CXX_COMPILER=${LIBDIR}/dpcpp/bin/clang++.exe
      -DCMAKE_C_COMPILER=${LIBDIR}/dpcpp/bin/clang.exe
      -DCMAKE_SHARED_LINKER_FLAGS=-L"${LIBDIR}/dpcpp/lib"
      -DEMBREE_SYCL_SUPPORT=ON
      ${EMBREE_EXTRA_ARGS}
    )
  else()
    set(EMBREE_EXTRA_ARGS
      -DCMAKE_CXX_COMPILER=${LIBDIR}/dpcpp/bin/clang++
      -DCMAKE_C_COMPILER=${LIBDIR}/dpcpp/bin/clang
      -DCMAKE_SHARED_LINKER_FLAGS=-L"${LIBDIR}/dpcpp/lib"
      -DEMBREE_SYCL_SUPPORT=ON
      ${EMBREE_EXTRA_ARGS}
    )
  endif()
endif()

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
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/embree

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -d
      ${BUILD_DIR}/embree/src/external_embree <
      ${PATCH_DIR}/embree.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/embree
    ${EMBREE_CMAKE_FLAGS}
    ${EMBREE_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/embree
)

if(NOT APPLE AND NOT BLENDER_PLATFORM_ARM)
  add_dependencies(
    external_embree
    external_tbb
    external_dpcpp
  )
else()
  add_dependencies(
    external_embree
    external_tbb
  )
endif()

if(WIN32)
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
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/embree/lib/embree4_sycl_d.lib
        ${HARVEST_TARGET}/embree/lib/embree4_sycl_d.lib

      DEPENDEES install
    )
  endif()
else()
  harvest(external_embree embree/include embree/include "*.h")
  harvest(external_embree embree/lib embree/lib "*.a")
  harvest_rpath_lib(external_embree embree/lib embree/lib "*${SHAREDLIBEXT}*")
endif()
