# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_zlib
  URL file://${PACKAGE_DIR}/${ZLIB_FILE}
  URL_HASH ${ZLIB_HASH_TYPE}=${ZLIB_HASH}
  PREFIX ${BUILD_DIR}/zlib
  CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zlib ${DEFAULT_CMAKE_FLAGS}
  INSTALL_DIR ${LIBDIR}/zlib
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_zlib after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstatic${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st${LIBEXT}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zlib/include/ ${HARVEST_TARGET}/zlib/include/
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_zlib after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstaticd${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st_d${LIBEXT}
    DEPENDEES install
    )
  endif()
else()
  ExternalProject_Add_Step(external_zlib after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/libz.a ${LIBDIR}/zlib/lib/libz_pic.a
    DEPENDEES install
  )
endif()
