# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(ICONV_EXTRA_ARGS)

ExternalProject_Add(external_iconv
  URL file://${PACKAGE_DIR}/${ICONV_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ICONV_HASH_TYPE}=${ICONV_HASH}
  PREFIX ${BUILD_DIR}/iconv
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && ${CONFIGURE_COMMAND} --enable-static --prefix=${mingw_LIBDIR}/iconv
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/iconv/src/external_iconv/ && make install
  INSTALL_DIR ${LIBDIR}/iconv
)

if(MSVC)
  set_target_properties(external_iconv PROPERTIES FOLDER Mingw)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_iconv after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/lib/libiconv.a ${HARVEST_TARGET}/iconv/lib/iconv.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/include/iconv.h ${HARVEST_TARGET}/iconv/include/iconv.h
      DEPENDEES install
    )
  endif()
endif()
