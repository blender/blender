# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# When cross-compiling use crossenv to create a Python cross-compiling environment, used for numpy,
# zstandard and site-packages. The crossenv module is fetched from the host Python site-packages.

# Cross-env python wrapper, equivalent to PYTHON_BINARY when not cross-compiling.
set(PYTHON_CROSSENV_BINARY ${PYTHON_BINARY})

if(CMAKE_CROSSCOMPILING)
  set(PYTHON_CROSSENV_BINARY ${PYTHON_CROSSENV_DIR}/cross/bin/python${PYTHON_SHORT_VERSION})
  # Append the target site package to the PYTHONPATH for site-packages to find each other during cross-compilation,
  # (e.g. for numpy to find meson-build). Without this, pip/setup.py only sees the (empty) local venv site-package
  # directory.
  set(PYTHON_CROSSENV_ENV "PYTHONPATH:=${LIBDIR}/python/lib/python${PYTHON_SHORT_VERSION}/site-packages")

  ExternalProject_Add(external_python_crossenv
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    PREFIX ${BUILD_DIR}/python_crossenv

    BUILD_COMMAND ${PYTHON_BINARY} -m crossenv
      --env ${PYTHON_CROSSENV_ENV}
      ${LIBDIR}/python/bin/python${PYTHON_SHORT_VERSION}
      ${BUILD_DIR}/python_crossenv
  )

  add_dependencies(
    external_python_crossenv
    external_python
  )
endif()

