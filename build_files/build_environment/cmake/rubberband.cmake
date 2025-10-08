# # SPDX-FileCopyrightText: 2025 Blender Authors
# #
# # SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(RUBBERBAND_CONFIGURE_ENV ${CONFIGURE_ENV_MSVC})
  set(FFTW_DIR ${LIBDIR}/fftw3/lib/pkgconfig)
  file(TO_NATIVE_PATH "${FFTW_DIR}" FFTW_DIR)
  set(RUBBERBAND_PKG_ENV PKG_CONFIG_PATH=${FFTW_DIR})
else()
  set(RUBBERBAND_CONFIGURE_ENV ${CONFIGURE_ENV})
  set(RUBBERBAND_PKG_ENV "PKG_CONFIG_PATH=\
${LIBDIR}/fftw3/lib/pkgconfig:\
$PKG_CONFIG_PATH"
  )
endif()

ExternalProject_Add(external_rubberband
  URL file://${PACKAGE_DIR}/${RUBBERBAND_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${RUBBERBAND_HASH_TYPE}=${RUBBERBAND_HASH}
  PREFIX ${BUILD_DIR}/rubberband

  CONFIGURE_COMMAND ${RUBBERBAND_CONFIGURE_ENV} &&
    ${CMAKE_COMMAND} -E env ${RUBBERBAND_PKG_ENV} ${MESON} setup
      --prefix ${LIBDIR}/rubberband
      --libdir lib
      ${MESON_BUILD_TYPE}
      -Dauto_features=disabled
      -Ddefault_library=static
      -Dfft=fftw
      ${BUILD_DIR}/rubberband/src/external_rubberband-build
      ${BUILD_DIR}/rubberband/src/external_rubberband

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
  INSTALL_DIR ${LIBDIR}/rubberband
)

add_dependencies(
  external_rubberband
  external_fftw
  # Needed for `MESON`.
  external_python_site_packages
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_rubberband after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/rubberband/include
        ${HARVEST_TARGET}/rubberband/include
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/rubberband/lib/rubberband-static.lib
        ${HARVEST_TARGET}/rubberband/lib/rubberband-static.lib
      DEPENDEES install
    )
  else()
    ExternalProject_Add_Step(external_rubberband after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/rubberband/lib/rubberband-static.lib
        ${HARVEST_TARGET}/rubberband/lib/rubberband-static_d.lib
      DEPENDEES install
    )
  endif()
else()
  harvest(external_rubberband rubberband/include rubberband/include "*.h")
  harvest(external_rubberband rubberband/lib rubberband/lib "*.a")
endif()
