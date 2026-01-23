# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  message(STATUS "BIN >${PYTHON_BINARY}<")
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

if(WIN32)
  file(WRITE ${CMAKE_BINARY_DIR}/fix_path.bat
    "set PATH=${LIBDIR}/python;${LIBDIR}/python/scripts;%PATH%\n"
  )
  set(NUMPY_CONF ${CMAKE_BINARY_DIR}/fix_path.bat)
else()
  set(NUMPY_CONF export CYTHON=${LIBDIR}/python/bin/cython)
endif()

ExternalProject_Add(external_numpy
  URL file://${PACKAGE_DIR}/${NUMPY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NUMPY_HASH_TYPE}=${NUMPY_HASH}
  PREFIX ${BUILD_DIR}/numpy
  PATCH_COMMAND ${NUMPY_PATCH}
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1

  BUILD_COMMAND ${NUMPY_CONF} && ${PYTHON_BINARY} -m pip install --no-build-isolation .

  INSTALL_COMMAND ""
)

add_dependencies(
  external_numpy
  external_python
  external_python_site_packages
  external_cython
)
