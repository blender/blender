# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(CONFIGURE_ENV ${CONFIGURE_ENV_MSVC})
endif()

ExternalProject_Add(external_fribidi
  URL file://${PACKAGE_DIR}/${FRIBIDI_FILE}
  URL_HASH ${FRIBIDI_HASH_TYPE}=${FRIBIDI_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/fribidi

  CONFIGURE_COMMAND
    ${MESON} setup
      --prefix ${LIBDIR}/fribidi
      ${MESON_BUILD_TYPE}
      -Ddocs=false
      --default-library static
      --libdir lib
      ${BUILD_DIR}/fribidi/src/external_fribidi-build
      ${BUILD_DIR}/fribidi/src/external_fribidi

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
  INSTALL_DIR ${LIBDIR}/fribidi
)

add_dependencies(
  external_fribidi
  external_python
  # Needed for `MESON`.
  external_python_site_packages
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_fribidi after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/fribidi/include
      ${HARVEST_TARGET}/fribidi/include
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/fribidi/lib/libfribidi.a
      ${HARVEST_TARGET}/fribidi/lib/libfribidi.lib

    DEPENDEES install
  )
endif()
