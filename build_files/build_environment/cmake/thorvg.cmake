# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(THORVG_EXTRA_OPTIONS
  -Dloaders=svg
  -Dextra=  # Set to none, disable building with OpenMP
)

ExternalProject_Add(external_thorvg
  URL file://${PACKAGE_DIR}/${THORVG_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${THORVG_HASH_TYPE}=${THORVG_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/thorvg

  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    ${MESON} setup
      --prefix ${LIBDIR}/thorvg
      --libdir lib
      --default-library static
      ${MESON_BUILD_TYPE}
      ${THORVG_EXTRA_OPTIONS}
      ${BUILD_DIR}/thorvg/src/external_thorvg-build
      ${BUILD_DIR}/thorvg/src/external_thorvg

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install

  INSTALL_DIR ${LIBDIR}/thorvg
)

add_dependencies(
  external_thorvg
  external_python
  # Needed for `MESON`.
  external_python_site_packages
)


if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_thorvg after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/thorvg/include
        ${HARVEST_TARGET}/thorvg/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/thorvg/lib/libthorvg-1.a
        ${HARVEST_TARGET}/thorvg/lib/libthorvg-1.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_thorvg after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/thorvg/lib/libthorvg-1.a
        ${HARVEST_TARGET}/thorvg/lib/libthorvg-1_d.lib
      DEPENDEES install
    )
  endif()
else()
  harvest(external_thorvg thorvg/include thorvg/include "*.h")
  harvest(external_thorvg thorvg/lib thorvg/lib "*.a")
endif()
