# SPDX-License-Identifier: GPL-2.0-or-later

set(NANOVDB_EXTRA_ARGS
   # NanoVDB is header-only, so only need the install target
  -DNANOVDB_BUILD_UNITTESTS=OFF
  -DNANOVDB_BUILD_EXAMPLES=OFF
  -DNANOVDB_BUILD_BENCHMARK=OFF
  -DNANOVDB_BUILD_DOCS=OFF
  -DNANOVDB_BUILD_TOOLS=OFF
  -DNANOVDB_CUDA_KEEP_PTX=OFF
   # Do not need to include any of the dependencies because of this
  -DNANOVDB_USE_OPENVDB=OFF
  -DNANOVDB_USE_OPENGL=OFF
  -DNANOVDB_USE_OPENCL=OFF
  -DNANOVDB_USE_CUDA=OFF
  -DNANOVDB_USE_TBB=OFF
  -DNANOVDB_USE_BLOSC=OFF
  -DNANOVDB_USE_ZLIB=OFF
  -DNANOVDB_USE_OPTIX=OFF
  -DNANOVDB_ALLOW_FETCHCONTENT=OFF
)

ExternalProject_Add(nanovdb
  URL file://${PACKAGE_DIR}/${NANOVDB_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NANOVDB_HASH_TYPE}=${NANOVDB_HASH}
  PREFIX ${BUILD_DIR}/nanovdb
  SOURCE_SUBDIR nanovdb
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/nanovdb/src/nanovdb < ${PATCH_DIR}/nanovdb.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/nanovdb ${DEFAULT_CMAKE_FLAGS} ${NANOVDB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/nanovdb
)

if(WIN32)
  ExternalProject_Add_Step(nanovdb after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/nanovdb/nanovdb ${HARVEST_TARGET}/nanovdb/include/nanovdb
    DEPENDEES install
  )
endif()
