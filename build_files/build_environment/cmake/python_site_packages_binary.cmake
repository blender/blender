# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

configure_file(${CMAKE_SOURCE_DIR}/cmake/python_binary_requirements.txt.in ${CMAKE_BINARY_DIR}/python_binary_requirements.txt @ONLY)


# `--require-hashes` accomplishes 2 things for us 
#
# - it forces us to supply hashses and versions for all packages installed protecting against supply chain attacks
# - if during a version bump any deps gain additional deps, these won't be in our requirements file, thus miss the hashes for those deps
#   and an error will occur alerting us to this situation. 

ExternalProject_Add(external_python_site_packages_binary
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  PREFIX ${BUILD_DIR}/site_packages

  INSTALL_COMMAND ${PYTHON_BINARY} -m pip install --no-cache-dir -r ${CMAKE_BINARY_DIR}/python_binary_requirements.txt
  --require-hashes
  --only-binary :all:
)

add_dependencies(
  external_python_site_packages_binary
  external_python
)
