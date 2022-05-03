# SPDX-License-Identifier: GPL-2.0-or-later

if(BUILD_MODE STREQUAL Debug)
  set(BLOSC_POST _d)
endif()

if(WIN32)
  set(OPENVDB_SHARED ON)
  set(OPENVDB_STATIC OFF)
else()
  set(OPENVDB_SHARED OFF)
  set(OPENVDB_STATIC ON)
endif()

set(OPENVDB_EXTRA_ARGS
  ${DEFAULT_BOOST_FLAGS}
  -DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
  -DBoost_USE_MULTITHREADED=ON
  -DBoost_USE_STATIC_LIBS=ON
  -DBoost_USE_STATIC_RUNTIME=OFF
  -DBOOST_ROOT=${LIBDIR}/boost
  -DBoost_NO_SYSTEM_PATHS=ON
  -DBoost_NO_BOOST_CMAKE=ON
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DBlosc_INCLUDE_DIR=${LIBDIR}/blosc/include/
  -DBlosc_LIBRARY=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DBlosc_LIBRARY_RELEASE=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DBlosc_LIBRARY_DEBUG=${LIBDIR}/blosc/lib/libblosc${BLOSC_POST}${LIBEXT}
  -DOPENVDB_BUILD_UNITTESTS=OFF
  -DOPENVDB_BUILD_PYTHON_MODULE=OFF
  -DOPENVDB_BUILD_NANOVDB=ON
  -DNANOVDB_BUILD_TOOLS=OFF
  -DBlosc_ROOT=${LIBDIR}/blosc/
  -DTBB_ROOT=${LIBDIR}/tbb/
  -DOPENVDB_CORE_SHARED=${OPENVDB_SHARED}
  -DOPENVDB_CORE_STATIC=${OPENVDB_STATIC}
  -DOPENVDB_BUILD_BINARIES=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
  -DBLOSC_USE_STATIC_LIBS=ON
  -DUSE_NANOVDB=ON
)

if(WIN32)
  # Namespaces seem to be buggy and cause linker errors due to things not
  # being in the correct namespace
  # needs to link pthreads due to it being a blosc dependency
  set(OPENVDB_EXTRA_ARGS ${OPENVDB_EXTRA_ARGS}
    -DCMAKE_CXX_STANDARD_LIBRARIES="${LIBDIR}/pthreads/lib/pthreadVC3.lib"
  )
endif()

ExternalProject_Add(openvdb
  URL file://${PACKAGE_DIR}/${OPENVDB_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENVDB_HASH_TYPE}=${OPENVDB_HASH}
  PREFIX ${BUILD_DIR}/openvdb
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/openvdb/src/openvdb < ${PATCH_DIR}/openvdb.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openvdb ${DEFAULT_CMAKE_FLAGS} ${OPENVDB_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/openvdb
)

add_dependencies(
  openvdb
  external_tbb
  external_boost
  external_zlib
  external_blosc
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(openvdb after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openvdb/include ${HARVEST_TARGET}/openvdb/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openvdb/lib/openvdb.lib ${HARVEST_TARGET}/openvdb/lib/openvdb.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openvdb/bin/openvdb.dll ${HARVEST_TARGET}/openvdb/bin/openvdb.dll
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(openvdb after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openvdb/lib/openvdb_d.lib ${HARVEST_TARGET}/openvdb/lib/openvdb_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openvdb/bin/openvdb_d.dll ${HARVEST_TARGET}/openvdb/bin/openvdb_d.dll
      DEPENDEES install
    )
  endif()
endif()
