# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  if(BUILD_MODE STREQUAL Debug)
    set(ZSTANDARD_DIR_POSTFIX -pydebug)
    set(ZSTANDARD_ARCHIVE_POSTFIX d)
    set(ZSTANDARD_BUILD_OPTION --debug)
  else()
    set(ZSTANDARD_DIR_POSTFIX)
    set(ZSTANDARD_ARCHIVE_POSTFIX)
    set(ZSTANDARD_BUILD_OPTION)
  endif()
endif()

set(ZSTANDARD_POSTFIX)

ExternalProject_Add(external_zstandard
  URL file://${PACKAGE_DIR}/${ZSTANDARD_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${ZSTANDARD_HASH_TYPE}=${ZSTANDARD_HASH}
  PREFIX ${BUILD_DIR}/zstandard
  PATCH_COMMAND ${ZSTANDARD_PATCH}
  CONFIGURE_COMMAND ""
  LOG_BUILD 1
  BUILD_IN_SOURCE 1

  BUILD_COMMAND
    ${PYTHON_BINARY} setup.py
      --system-zstd
      build_ext ${ZSTANDARD_BUILD_OPTION} -j${PYTHON_MAKE_THREADS}
      --include-dirs=${LIBDIR}/zstd/include
      --library-dirs=${LIBDIR}/zstd/lib
      install
      --old-and-unmanageable

  INSTALL_COMMAND ""
)

add_dependencies(
  external_zstandard
  external_python
  external_zstd
)
