# SPDX-License-Identifier: GPL-2.0-or-later

set(IMATH_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=OFF
  -DBUILD_TESTING=OFF
  -DIMATH_LIB_SUFFIX=${OPENEXR_VERSION_BUILD_POSTFIX}
)

ExternalProject_Add(external_imath
  URL file://${PACKAGE_DIR}/${IMATH_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${IMATH_HASH_TYPE}=${IMATH_HASH}
  PREFIX ${BUILD_DIR}/imath
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/imath ${DEFAULT_CMAKE_FLAGS} ${IMATH_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/imath
)

if(WIN32)
  ExternalProject_Add_Step(external_imath after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/imath/lib ${HARVEST_TARGET}/imath/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/imath/include ${HARVEST_TARGET}/imath/include
    DEPENDEES install
  )
endif()
