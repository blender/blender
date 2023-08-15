# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32 AND BUILD_MODE STREQUAL Debug)
  set(SITE_PACKAGES_EXTRA --global-option build --global-option --debug)
  # zstandard is determined to build and link release mode libs in a debug
  # configuration, the only way to make it happy is to bend to its will
  # and give it a library to link with.
  set(
    PIP_CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy
    ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}_d.lib
    ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib
  )
else()
  set(PIP_CONFIGURE_COMMAND echo ".")
endif()

ExternalProject_Add(external_python_site_packages
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND ${PIP_CONFIGURE_COMMAND}
  BUILD_COMMAND ""
  PREFIX ${BUILD_DIR}/site_packages

  # setuptools is downgraded to 63.2.0 (same as python 3.10.8) since numpy 1.23.x seemingly has
  # issues building on windows with the newer versions that ships with python 3.10.9+
  INSTALL_COMMAND ${PYTHON_BINARY} -m pip install --no-cache-dir ${SITE_PACKAGES_EXTRA}
  setuptools==63.2.0
  cython==${CYTHON_VERSION}
  idna==${IDNA_VERSION}
  charset-normalizer==${CHARSET_NORMALIZER_VERSION}
  urllib3==${URLLIB3_VERSION}
  certifi==${CERTIFI_VERSION}
  requests==${REQUESTS_VERSION}
  zstandard==${ZSTANDARD_VERSION}
  autopep8==${AUTOPEP8_VERSION}
  pycodestyle==${PYCODESTYLE_VERSION}
  toml==${TOML_VERSION}
  meson==${MESON_VERSION}
  --no-binary :all:
)

add_dependencies(
  external_python_site_packages
  external_python
)
