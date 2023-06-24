# SPDX-FileCopyrightText: 2002-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  set(ZLIB_PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/zlib/src/external_zlib < ${PATCH_DIR}/zlib.diff)
else()
  set(ZLIB_PATCH_COMMAND echo .)
endif()

ExternalProject_Add(external_zlib
  URL file://${PACKAGE_DIR}/${ZLIB_FILE}
  URL_HASH ${ZLIB_HASH_TYPE}=${ZLIB_HASH}
  PREFIX ${BUILD_DIR}/zlib
  PATCH_COMMAND ${ZLIB_PATCH_COMMAND}
  CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/zlib ${DEFAULT_CMAKE_FLAGS}
  INSTALL_DIR ${LIBDIR}/zlib
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_zlib after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstatic${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st${LIBEXT}
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zlib/include/ ${HARVEST_TARGET}/zlib/include/
      COMMAND ${CMAKE_COMMAND} -E rm -f ${LIBDIR}/zlib/bin/zlib.dll
      COMMAND ${CMAKE_COMMAND} -E rm -f ${LIBDIR}/zlib/lib/zlib.lib
	    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstatic${LIBEXT} ${LIBDIR}/zlib/lib/zlib${LIBEXT}
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_zlib after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstaticd${LIBEXT} ${HARVEST_TARGET}/zlib/lib/libz_st_d${LIBEXT}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${LIBDIR}/zlib/bin/zlib.dll
    COMMAND ${CMAKE_COMMAND} -E rm -f ${LIBDIR}/zlib/lib/zlib.lib
    DEPENDEES install
    )
  endif()
else()
  ExternalProject_Add_Step(external_zlib after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/libz.a ${LIBDIR}/zlib/lib/libz_pic.a
    DEPENDEES install
  )
endif()
