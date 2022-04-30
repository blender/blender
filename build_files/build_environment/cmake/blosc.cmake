# SPDX-License-Identifier: GPL-2.0-or-later

set(BLOSC_EXTRA_ARGS
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DBUILD_TESTS=OFF
  -DBUILD_BENCHMARKS=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
  -DThreads_FOUND=1
  -DPTHREAD_LIBS=${LIBDIR}/pthreads/lib/pthreadVC3.lib
  -DPTHREAD_INCLUDE_DIR=${LIBDIR}/pthreads/inc
  -DDEACTIVATE_SNAPPY=ON
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
)

# Prevent blosc from including its own local copy of zlib in the object file
# and cause linker errors with everybody else.
set(BLOSC_EXTRA_ARGS ${BLOSC_EXTRA_ARGS}
  -DPREFER_EXTERNAL_ZLIB=ON
)

ExternalProject_Add(external_blosc
  URL file://${PACKAGE_DIR}/${BLOSC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${BLOSC_HASH_TYPE}=${BLOSC_HASH}
  PREFIX ${BUILD_DIR}/blosc
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/blosc ${DEFAULT_CMAKE_FLAGS} ${BLOSC_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/blosc
)

add_dependencies(
  external_blosc
  external_zlib
)
if(WIN32)
  add_dependencies(
    external_blosc
    external_pthreads
  )
endif()

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_blosc after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc.lib ${HARVEST_TARGET}/blosc/lib/libblosc.lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/blosc/include/ ${HARVEST_TARGET}/blosc/include/
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_blosc after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc_d.lib ${HARVEST_TARGET}/blosc/lib/libblosc_d.lib
      DEPENDEES install
    )
  endif()
endif()
