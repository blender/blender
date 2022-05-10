# SPDX-License-Identifier: GPL-2.0-or-later

set(TIFF_EXTRA_ARGS
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include
  -DPNG_STATIC=ON
  -DBUILD_SHARED_LIBS=OFF
  -Dlzma=OFF
  -Djbig=OFF
  -Dzstd=OFF
  -Dwebp=OFF
)

ExternalProject_Add(external_tiff
  URL file://${PACKAGE_DIR}/${TIFF_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${TIFF_HASH_TYPE}=${TIFF_HASH}
  PREFIX ${BUILD_DIR}/tiff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/tiff ${DEFAULT_CMAKE_FLAGS} ${TIFF_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/tiff
)

add_dependencies(
  external_tiff
  external_zlib
)

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  ExternalProject_Add_Step(external_tiff after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/tiff/lib/tiffd${LIBEXT} ${LIBDIR}/tiff/lib/tiff${LIBEXT}
    DEPENDEES install
  )
endif()
