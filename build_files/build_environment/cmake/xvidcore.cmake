# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(XVIDCORE_EXTRA_ARGS --host=${MINGW_HOST})
endif()

ExternalProject_Add(external_xvidcore
  URL file://${PACKAGE_DIR}/${XVIDCORE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${XVIDCORE_HASH_TYPE}=${XVIDCORE_HASH}
  PREFIX ${BUILD_DIR}/xvidcore
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/xvidcore ${XVIDCORE_EXTRA_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} &&
  ${CMAKE_COMMAND} -E remove ${LIBDIR}/xvidcore/lib/* && # clean because re-installing fails otherwise
  cd ${BUILD_DIR}/xvidcore/src/external_xvidcore/build/generic && make install
  INSTALL_DIR ${LIBDIR}/xvidcore
)

if(WIN32)
  ExternalProject_Add_Step(external_xvidcore after_install
    COMMAND ${CMAKE_COMMAND} -E rename ${LIBDIR}/xvidcore/lib/xvidcore.a ${LIBDIR}/xvidcore/lib/libxvidcore.a || true
    COMMAND ${CMAKE_COMMAND} -E remove ${LIBDIR}/xvidcore/lib/xvidcore.dll.a
    DEPENDEES install
  )
endif()

if(MSVC)
  set_target_properties(external_xvidcore PROPERTIES FOLDER Mingw)
endif()
