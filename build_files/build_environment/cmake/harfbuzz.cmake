# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(HARFBUZZ_CONFIGURE_ENV ${CONFIGURE_ENV_MSVC})
  set(HARFBUZZ_PKG_ENV FREETYPE_DIR=${LIBDIR}/freetype)
else()
  set(HARFBUZZ_CONFIGURE_ENV ${CONFIGURE_ENV})
  set(HARFBUZZ_PKG_ENV "PKG_CONFIG_PATH=\
${LIBDIR}/freetype/lib/pkgconfig:\
${LIBDIR}/brotli/lib/pkgconfig:\
${LIBDIR}/lib/python3.10/pkgconfig:\
$PKG_CONFIG_PATH"
  )
endif()

set(HARFBUZZ_EXTRA_OPTIONS
  -Dtests=disabled
  -Dfreetype=enabled
  -Dglib=disabled
  -Dgobject=disabled
  # Only used for command line utilities,
  # disable as this would add an addition & unnecessary build-dependency.
  -Dcairo=disabled
  ${MESON_BUILD_TYPE}
)

ExternalProject_Add(external_harfbuzz
  URL file://${PACKAGE_DIR}/${HARFBUZZ_FILE}
  URL_HASH ${HARFBUZZ_HASH_TYPE}=${HARFBUZZ_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/harfbuzz

  CONFIGURE_COMMAND ${HARFBUZZ_CONFIGURE_ENV} &&
    ${CMAKE_COMMAND} -E env ${HARFBUZZ_PKG_ENV} ${MESON} setup
      --prefix ${LIBDIR}/harfbuzz ${HARFBUZZ_EXTRA_OPTIONS}
      --default-library static
      --libdir lib
      ${BUILD_DIR}/harfbuzz/src/external_harfbuzz-build
      ${BUILD_DIR}/harfbuzz/src/external_harfbuzz

  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
  INSTALL_DIR ${LIBDIR}/harfbuzz
)

add_dependencies(
  external_harfbuzz
  external_python
  external_freetype
  # Needed for `MESON`.
  external_python_site_packages
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_harfbuzz after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${LIBDIR}/harfbuzz/include
      ${HARVEST_TARGET}/harfbuzz/include
    # We do not use the subset API currently, so copying only the main library will suffice for now
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/harfbuzz/lib/libharfbuzz.a
      ${HARVEST_TARGET}/harfbuzz/lib/libharfbuzz.lib
    DEPENDEES install
  )
endif()

if(BUILD_MODE STREQUAL Debug AND WIN32)
  ExternalProject_Add_Step(external_harfbuzz after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/harfbuzz/lib/libharfbuzz.a
      ${HARVEST_TARGET}/harfbuzz/lib/libharfbuzz_d.lib
    DEPENDEES install
  )
endif()
