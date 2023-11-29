# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

ExternalProject_Add(external_vorbis
    URL file://${PACKAGE_DIR}/${VORBIS_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${VORBIS_HASH_TYPE}=${VORBIS_HASH}
    PREFIX ${BUILD_DIR}/vorbis
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/vorbis -DOGG_ROOT=${LIBDIR}/ogg ${DEFAULT_CMAKE_FLAGS}
    INSTALL_DIR ${LIBDIR}/vorbis
  )

add_dependencies(
  external_vorbis
  external_ogg
)
