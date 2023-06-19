# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(HARU_EXTRA_ARGS
  -DLIBHPDF_SHARED=OFF
  -DLIBHPDF_STATIC=ON
  -DLIBHPDF_EXAMPLES=OFF
  -DLIBHPDF_ENABLE_EXCEPTIONS=ON
)

ExternalProject_Add(external_haru
  URL file://${PACKAGE_DIR}/${HARU_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${HARU_HASH_TYPE}=${HARU_HASH}
  PREFIX ${BUILD_DIR}/haru
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/haru/src/external_haru < ${PATCH_DIR}/haru.diff
  CMAKE_ARGS
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=${LIBDIR}/haru
    ${DEFAULT_CMAKE_FLAGS} ${HARU_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/haru
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_haru after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/haru/include ${HARVEST_TARGET}/haru/include
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/haru/lib/libhpdfs.lib ${HARVEST_TARGET}/haru/lib/libhpdfs.lib
      DEPENDEES install
    )
  endif()
endif()
