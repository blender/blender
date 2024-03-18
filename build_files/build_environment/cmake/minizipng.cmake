# SPDX-FileCopyrightText: 2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(MINIZIPNG_EXTRA_ARGS
  # Disable most compression methods like OpenColorIO does, enabling
  # more is not needed and will give link errors.
  -DMZ_FETCH_LIBS=OFF
  -DMZ_LIBCOMP=OFF
  -DMZ_PKCRYPT=OFF
  -DMZ_WZAES=OFF
  -DMZ_OPENSSL=OFF
  -DMZ_SIGNING=OFF
  -DMZ_LZMA=OFF
  -DMZ_ZSTD=OFF
  -DMZ_BZIP2=OFF
  -DMZ_ICONV=OFF
  -DMZ_ZLIB=ON
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  # Because OCIO hardcodes a non standard include path
  -DCMAKE_INSTALL_INCLUDEDIR=${LIBDIR}/minizipng/include/minizip-ng
)

ExternalProject_Add(external_minizipng
  URL file://${PACKAGE_DIR}/${MINIZIPNG_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${MINIZIPNG_HASH_TYPE}=${MINIZIPNG_HASH}
  PREFIX ${BUILD_DIR}/minizipng
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/minizipng ${DEFAULT_CMAKE_FLAGS} ${MINIZIPNG_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/minizipng
)

add_dependencies(
  external_minizipng
  external_zlib
)
