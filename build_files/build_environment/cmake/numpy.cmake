# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(MSVC)
  if(BUILD_MODE STREQUAL Debug)
    set(NUMPY_DIR_POSTFIX -pydebug)
    set(NUMPY_ARCHIVE_POSTFIX d)
    set(NUMPY_BUILD_OPTION --debug)
  else()
    set(NUMPY_DIR_POSTFIX "")
    set(NUMPY_ARCHIVE_POSTFIX "")
    set(NUMPY_BUILD_OPTION "")
  endif()
endif()

set(NUMPY_POSTFIX "")

if(WIN32)
  file(WRITE ${CMAKE_BINARY_DIR}/fix_path.bat
    "set PATH=${LIBDIR}/python;${LIBDIR}/python/scripts;%PATH%\n"
  )
  set(NUMPY_CONF ${CMAKE_BINARY_DIR}/fix_path.bat)
else()
  set(NUMPY_CONF export CYTHON=${HOST_LIBDIR}/python/bin/cython)
endif()

ExternalProject_Add(external_numpy
  URL file://${PACKAGE_DIR}/${NUMPY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${NUMPY_HASH_TYPE}=${NUMPY_HASH}
  PREFIX ${BUILD_DIR}/numpy
  PATCH_COMMAND ${NUMPY_PATCH}
  CONFIGURE_COMMAND ""
  BUILD_IN_SOURCE 1

  # Use an explicit --prefix for cross-compilation builds, without which the library will get installed in
  # the python_crossenv site-package instead of the proper $LIBIDR.
  BUILD_COMMAND ${NUMPY_CONF} && ${PYTHON_CROSSENV_BINARY} -m pip install
      --no-build-isolation
      --prefix=${LIBDIR}/python
      .

  INSTALL_COMMAND ""
)

add_dependencies(
  external_numpy
  external_python
  external_python_site_packages
)

if(NOT CMAKE_CROSSCOMPILING)
  # Cython is a host build tool. Cross-compilation fetches it from HOST_LIBDIR, do not build while cross-compiling.
  add_dependencies(
    external_numpy
    external_cython
  )
endif()

if(CMAKE_CROSSCOMPILING)
  add_dependencies(
    external_numpy
    external_python_crossenv
  )
endif()
