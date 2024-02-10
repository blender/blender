# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(BUILD_MODE STREQUAL Debug)
  set(BLOSC_POST _d)
endif()

set(OPENVDB_EXTRA_ARGS
  ${DEFAULT_BOOST_FLAGS}
  -DUSE_STATIC_DEPENDENCIES=OFF   # This is the global toggle for static libs
  # Once the above switch is off, you can set it
  # for each individual library below.
  -DBLOSC_USE_STATIC_LIBS=ON
  -DTBB_USE_STATIC_LIBS=OFF
  -DBoost_USE_STATIC_LIBS=OFF
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DBlosc_INCLUDE_DIR=${LIBDIR}/blosc/include/
  -DBlosc_LIBRARY=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DBlosc_LIBRARY_RELEASE=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DBlosc_LIBRARY_DEBUG=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DOPENVDB_BUILD_UNITTESTS=OFF
  -DOPENVDB_BUILD_NANOVDB=ON
  -DNANOVDB_BUILD_TOOLS=OFF
  -DBlosc_ROOT=${LIBDIR}/blosc/
  -DTBB_ROOT=${LIBDIR}/tbb/
  -DTbb_INCLUDE_DIR=${LIBDIR}/tbb/include
  -DTbb_LEGACY_INCLUDE_DIR=${LIBDIR}/tbb/include
  -DOPENVDB_CORE_SHARED=ON
  -DOPENVDB_CORE_STATIC=OFF
  -DOPENVDB_BUILD_BINARIES=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
  -DBLOSC_USE_STATIC_LIBS=ON
  -DUSE_NANOVDB=ON
  -DOPENVDB_BUILD_PYTHON_MODULE=ON
  -DOPENVDB_PYTHON_WRAP_ALL_GRID_TYPES=ON
  -DUSE_NUMPY=ON
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -Dpybind11_DIR=${LIBDIR}/pybind11/share/cmake/pybind11

  # OPENVDB_AX Disabled for now as it adds ~25MB distribution wise
  # with no blender code depending on it, seems wasteful.
  # -DOPENVDB_BUILD_AX=ON
  # -DOPENVDB_AX_SHARED=ON
  # -DOPENVDB_AX_STATIC=OFF
  # -DLLVM_DIR=${LIBDIR}/llvm/lib/cmake/llvm
)

set(OPENVDB_PATCH
  ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/openvdb/src/openvdb <
    ${PATCH_DIR}/openvdb.diff &&
  ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/openvdb/src/openvdb <
    ${PATCH_DIR}/openvdb_1706.diff &&
  ${PATCH_CMD} -p 1 -d
    ${BUILD_DIR}/openvdb/src/openvdb <
    ${PATCH_DIR}/openvdb_1733.diff
)

ExternalProject_Add(openvdb
  URL file://${PACKAGE_DIR}/${OPENVDB_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENVDB_HASH_TYPE}=${OPENVDB_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openvdb
  PATCH_COMMAND ${OPENVDB_PATCH}

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openvdb
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENVDB_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/openvdb
)

add_dependencies(
  openvdb
  external_tbb
  external_boost
  external_zlib
  external_blosc
  external_python
  external_numpy
  external_pybind11
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(openvdb after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/openvdb/include
        ${HARVEST_TARGET}/openvdb/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/lib/openvdb.lib
        ${HARVEST_TARGET}/openvdb/lib/openvdb.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/bin/openvdb.dll
        ${HARVEST_TARGET}/openvdb/bin/openvdb.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/lib/python${PYTHON_SHORT_VERSION}/site-packages/pyopenvdb.cp${PYTHON_SHORT_VERSION_NO_DOTS}-win_amd64.pyd
        ${HARVEST_TARGET}openvdb/python/pyopenvdb.cp${PYTHON_SHORT_VERSION_NO_DOTS}-win_amd64.pyd

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(openvdb after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/lib/openvdb_d.lib
        ${HARVEST_TARGET}/openvdb/lib/openvdb_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/bin/openvdb_d.dll
        ${HARVEST_TARGET}/openvdb/bin/openvdb_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openvdb/lib/python${PYTHON_SHORT_VERSION}/site-packages/pyopenvdb_d.cp${PYTHON_SHORT_VERSION_NO_DOTS}-win_amd64.pyd
        ${HARVEST_TARGET}openvdb/python/pyopenvdb_d.cp${PYTHON_SHORT_VERSION_NO_DOTS}-win_amd64.pyd

      DEPENDEES install
    )
  endif()
endif()
