# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  message("BIN >${PYTHON_BINARY}<")
  if(BUILD_MODE STREQUAL Debug)
    set(NUMPY_DIR_POSTFIX -pydebug)
    set(NUMPY_ARCHIVE_POSTFIX d)
    set(NUMPY_BUILD_OPTION --debug)
  else()
    set(NUMPY_DIR_POSTFIX)
    set(NUMPY_ARCHIVE_POSTFIX)
    set(NUMPY_BUILD_OPTION)
  endif()
endif()

set(NUMPY_POSTFIX)

ExternalProject_Add(external_numpy
  URL file://${PACKAGE_DIR}/${NUMPY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NUMPY_HASH_TYPE}=${NUMPY_HASH}
  PREFIX ${BUILD_DIR}/numpy
  PATCH_COMMAND ${NUMPY_PATCH}
  CONFIGURE_COMMAND ""
  LOG_BUILD 1

  BUILD_COMMAND
    ${PYTHON_BINARY} ${BUILD_DIR}/numpy/src/external_numpy/setup.py
      build ${NUMPY_BUILD_OPTION}
      install
      --old-and-unmanageable

  INSTALL_COMMAND ""
)

add_dependencies(
  external_numpy
  external_python
  external_python_site_packages
)
