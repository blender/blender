# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(WEBP_EXTRA_ARGS
  -DWEBP_BUILD_ANIM_UTILS=OFF
  -DWEBP_BUILD_CWEBP=OFF
  -DWEBP_BUILD_DWEBP=OFF
  -DWEBP_BUILD_GIF2WEBP=OFF
  -DWEBP_BUILD_IMG2WEBP=OFF
  -DWEBP_BUILD_VWEBP=OFF
  -DWEBP_BUILD_WEBPINFO=OFF
  -DWEBP_BUILD_WEBPMUX=OFF
  -DWEBP_BUILD_EXTRAS=OFF
)

if(WIN32)
  set(WEBP_BUILD_DIR ${BUILD_MODE}/)
else()
  set(WEBP_BUILD_DIR)
endif()

ExternalProject_Add(external_webp
  URL file://${PACKAGE_DIR}/${WEBP_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${WEBP_HASH_TYPE}=${WEBP_HASH}
  PREFIX ${BUILD_DIR}/webp
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/webp -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${WEBP_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/webp
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_webp after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/webp ${HARVEST_TARGET}/webp
      DEPENDEES install
    )
  endif()
endif()
