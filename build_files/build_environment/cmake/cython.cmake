# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  if(BUILD_MODE STREQUAL Debug)
    set(CYTHON_DIR_POSTFIX -pydebug)
    set(CYTHON_ARCHIVE_POSTFIX d)
    set(CYTHON_BUILD_OPTION --debug)
  else()
    set(CYTHON_DIR_POSTFIX)
    set(CYTHON_ARCHIVE_POSTFIX)
    set(CYTHON_BUILD_OPTION)
  endif()
endif()

set(CYTHON_POSTFIX)

ExternalProject_Add(external_cython
  URL file://${PACKAGE_DIR}/${CYTHON_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${CYTHON_HASH_TYPE}=${CYTHON_HASH}
  PREFIX ${BUILD_DIR}/cython
  PATCH_COMMAND ${CYTHON_PATCH}
  CONFIGURE_COMMAND ""
  LOG_BUILD 1
  BUILD_IN_SOURCE 1

  BUILD_COMMAND
    ${PYTHON_BINARY} setup.py
      build ${CYTHON_BUILD_OPTION} -j${PYTHON_MAKE_THREADS}
      install
      --old-and-unmanageable

  INSTALL_COMMAND ""
)

add_dependencies(
  external_cython
  external_python
)
