# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(HARU_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=OFF
  -DLIBHPDF_EXAMPLES=OFF
  -DLIBHPDF_ENABLE_EXCEPTIONS=ON
)

ExternalProject_Add(external_haru
  URL file://${PACKAGE_DIR}/${HARU_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${HARU_HASH_TYPE}=${HARU_HASH}
  PREFIX ${BUILD_DIR}/haru

  CMAKE_ARGS
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/haru
    ${DEFAULT_CMAKE_FLAGS} ${HARU_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/haru
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_haru after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/haru/include
        ${HARVEST_TARGET}/haru/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/haru/lib/hpdf.lib
        ${HARVEST_TARGET}/haru/lib/hpdf.lib

      DEPENDEES install
    )
  endif()
else()
  harvest(external_haru haru/include haru/include "*.h")
  harvest(external_haru haru/lib haru/lib "*.a")
endif()
